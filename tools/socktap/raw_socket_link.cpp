#include "raw_socket_link.hpp"
#include "vanetza/access/ethertype.hpp"
#include <vanetza/access/data_request.hpp>
#include <vanetza/net/ethernet_header.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/filter.h>
#include "rssi_reader.hpp"

#include <boost/asio.hpp>

#define MAXLINE 1024

using namespace vanetza;
using namespace std::chrono;

bool rssi_enabled = false;
std::mutex xmit_mtx; 

RawSocketLink::RawSocketLink(boost::asio::generic::raw_protocol::socket&& socket, const std::string& device_name, bool _rssi_enabled) :
    socket_(std::move(socket)), receive_buffer_(2048, 0x00),
    receive_endpoint_(socket_.local_endpoint())
{   
    // BPF filter
    struct sock_filter bpf_code[] = {
        { 0x28, 0, 0, 0x0000000c },  // ldh [12]
        { 0x15, 0, 1, 0x00008947 },  // jeq #0x8947, jt 2, jf 3
        { 0x06, 0, 0, 0x0000ffff },  // ret #65535
        { 0x06, 0, 0, 0x00000000 },  // ret #0
    };

    struct sock_fprog bpf_filter = {
        .len = sizeof(bpf_code)/sizeof(struct sock_filter), 
        .filter = bpf_code,
    };

    // Apply the BPF filter
    if (setsockopt(socket_.native_handle(), SOL_SOCKET, SO_ATTACH_FILTER, 
        &bpf_filter, sizeof(bpf_filter)) < 0) {
        std::cout << "Could not apply BPF filter to raw socket link. Proceeding with unfiltered socket." << std::endl;
    }

    rssi_enabled = _rssi_enabled;
    if (rssi_enabled) start_rssi_reader(device_name);
}

void RawSocketLink::request(const access::DataRequest& request, std::unique_ptr<ChunkPacket> packet)
{
    packet->layer(OsiLayer::Link) = create_ethernet_header(request.destination_addr, request.source_addr, request.ether_type);
    transmit(std::move(packet));
}

std::size_t RawSocketLink::transmit(std::unique_ptr<ChunkPacket> packet)
{
    std::array<vanetza::ByteBuffer, layers_> buffers_;
    std::array<boost::asio::const_buffer, layers_> const_buffers;
    for (auto& layer : osi_layer_range<OsiLayer::Physical, OsiLayer::Application>()) {
        const auto index = distance(OsiLayer::Physical, layer);
        packet->layer(layer).convert(buffers_[index]);
        const_buffers[index] = boost::asio::buffer(buffers_[index]);
    }

    xmit_mtx.lock();
    std::size_t res = socket_.send(const_buffers);
    xmit_mtx.unlock();
    return res;
}

void RawSocketLink::indicate(IndicationCallback callback)
{
    callback_ = callback;
}

void RawSocketLink::do_receive()
{
    while(true) {
        boost::system::error_code ec;
        std::size_t bytesReceived = socket_.receive_from(
            boost::asio::buffer(receive_buffer_), receive_endpoint_, 0, ec);
        on_read(ec, bytesReceived);
    }
}

void RawSocketLink::do_receive_radiotap()
{
    while(true) {
        boost::system::error_code ec;
        std::size_t bytesReceived = socket_.receive_from(
            boost::asio::buffer(receive_buffer_), receive_endpoint_, 0, ec);
        on_read_radiotap(ec, bytesReceived);
    }
}

void RawSocketLink::on_read(const boost::system::error_code& ec, std::size_t read_bytes)
{
    if (!ec) {
        std::cout << "DEBUG: Packet received on physical network" << std::endl;
        ByteBuffer buffer(receive_buffer_.begin(), receive_buffer_.begin() + read_bytes);
        CohesivePacket packet(std::move(buffer), OsiLayer::Physical);
        boost::optional<EthernetHeader> eth = parse_ethernet_header(packet);
        if(eth->type == access::ethertype::GeoNetworking) {
            double time_reception = (double) duration_cast< microseconds >(system_clock::now().time_since_epoch()).count() / 1000000.0;
            packet.rssi = -255;
            std::stringstream stream;
            stream << eth->source;
            std::string source_mac(stream.str());
            if(rssi_enabled) {
                int nrssi = get_rssi(source_mac);
                if (nrssi != 1) {
                    packet.rssi = nrssi;
                }
                packet.mcs = get_mcs();
                Survey s = get_survey();
                packet.frequency = s.frequency;
                packet.noise = s.noise;
                packet.chan_busy_time = s.chan_busy_time;
                packet.chan_rx_time = s.chan_rx_time;
                packet.chan_tx_time = s.chan_tx_time;
                packet.source = source_mac;
            }
            packet.time_received = time_reception;
            if (callback_ && eth) {
                callback_(std::move(packet), *eth);
            }
        }
    }
}

void RawSocketLink::on_read_radiotap(const boost::system::error_code& ec, std::size_t read_bytes)
{
    if (!ec) {
        ByteBuffer raw_buffer(receive_buffer_.begin(), receive_buffer_.begin() + read_bytes);

        // 1. Minimum sanity check for Radiotap header
        if (raw_buffer.size() < 4) return;

        // 2. Dynamically extract Radiotap header length (e.g., your 38 bytes)
        uint16_t radiotap_len = raw_buffer[2] | (raw_buffer[3] << 8);
        
        // Ensure buffer contains at least Radiotap + minimal 802.11 Data header (24 bytes)
        if (raw_buffer.size() < radiotap_len + 24) return; 

        // 3. Extract 802.11 Frame Control bytes
        uint8_t fc_byte0 = raw_buffer[radiotap_len];
        uint8_t fc_byte1 = raw_buffer[radiotap_len + 1];


        // First byte of 802.11 header contains type and subtype, 0x08 -> normal, 0x88 -> qos_frame
        // Check if it is an 802.11 Data frame (Type 2 is 0x08 in the low bits)
        if ((fc_byte0 & 0x0C) != 0x08) return; 

        // 4. Dynamically determine 802.11 MAC header length
        size_t mac_header_len = 24; // Base data header length

        // Check To DS (bit 0) and From DS (bit 1) in FC Byte 1
        bool to_ds = (fc_byte1 & 0x01) != 0;
        bool from_ds = (fc_byte1 & 0x02) != 0;
        if (to_ds && from_ds) {
            mac_header_len += 6; // Address 4 is present (WDS)
        }

        // Check QoS bit (Bit 7 of FC Byte 0)
        bool is_qos = (fc_byte0 & 0x80) != 0;
        if (is_qos) {
            std::cout << "Qos frame detected" << "\n";
            mac_header_len += 2; // QoS Control field present
        }

        // Ensure buffer contains Radiotap + dynamic 802.11 Header + LLC/SNAP (8 bytes)
        if (raw_buffer.size() < radiotap_len + mac_header_len + 8) return; 

        // 5. Extract MAC addresses
        // Addr1 (offset 4) = Receiver, Addr2 (offset 10) = Transmitter
        vanetza::MacAddress dest_mac, src_mac;
        std::copy_n(raw_buffer.begin() + radiotap_len + 4, 6, dest_mac.octets.begin());
        std::copy_n(raw_buffer.begin() + radiotap_len + 10, 6, src_mac.octets.begin());

        // 6. Parse LLC/SNAP header (8 bytes) to extract EtherType
        size_t llc_offset = radiotap_len + mac_header_len;
        
        if (raw_buffer[llc_offset] != 0xAA || raw_buffer[llc_offset + 1] != 0xAA || raw_buffer[llc_offset + 2] != 0x03) {
            return; // Not an LLC/SNAP packet
        }

        uint16_t ethertype = (raw_buffer[llc_offset + 6] << 8) | raw_buffer[llc_offset + 7];

        // 7. Reconstruct the EthernetHeader
        boost::optional<EthernetHeader> eth = EthernetHeader{};
        eth->destination = dest_mac;
        eth->source = src_mac;
        // eth->type = ethertype;

        // 8. Create the CohesivePacket starting exactly where the network payload begins
        size_t payload_offset = llc_offset + 8;
        ByteBuffer payload_buffer(raw_buffer.begin() + payload_offset, raw_buffer.end());
        CohesivePacket packet(std::move(payload_buffer), OsiLayer::Network);

        // 9. Process GeoNetworking Data
        if(true || eth->type == vanetza::access::ethertype::GeoNetworking) {
            double time_reception = (double) duration_cast< microseconds >(system_clock::now().time_since_epoch()).count() / 1000000.0;
            packet.rssi = -255;
            
            std::stringstream stream;
            stream << eth->source;
            std::string source_mac(stream.str());
            
            if(rssi_enabled) {
                int nrssi = get_rssi(source_mac);
                if (nrssi != 1) {
                    packet.rssi = nrssi;
                }
                packet.mcs = get_mcs();
                Survey s = get_survey();
                packet.frequency = s.frequency;
                packet.noise = s.noise;
                packet.chan_busy_time = s.chan_busy_time;
                packet.chan_rx_time = s.chan_rx_time;
                packet.chan_tx_time = s.chan_tx_time;
                packet.source = source_mac;
            }
            
            packet.time_received = time_reception;
            
            if (callback_ && eth) {
                callback_(std::move(packet), *eth);
            }
        }
    }
}

boost::optional<EthernetHeader> RawSocketLink::parse_ethernet_header(vanetza::CohesivePacket& packet) const
{
    packet.set_boundary(OsiLayer::Physical, 0);
    if (packet.size(OsiLayer::Link) < EthernetHeader::length_bytes) {
        std::cerr << "Router dropped invalid packet (too short for Ethernet header)\n";
    } else {
        packet.set_boundary(OsiLayer::Link, EthernetHeader::length_bytes);
        auto link_range = packet[OsiLayer::Link];
        return decode_ethernet_header(link_range.begin(), link_range.end());
    }

    return boost::none;
}
