/**
 * Vanetza-compatible DDS CAM Publisher
 *
 * Uses fastddsgen-generated types:
 *
 *   struct JSONMessage {
 *       unsigned long uuid;
 *       unsigned long datetime;
 *       string topic;
 *       string message;
 *   };
 *
 *   struct EncodedITSMessage {
 *       string topic;
 *       sequence<octet> message;     // UPER-encoded ASN.1 CAM bytes
 *       @optional short rssi;
 *       @optional boolean newInfo;
 *       @optional short packetSize;
 *       @optional long stationID;
 *       @optional long receiverID;
 *       @optional short receiverType;
 *       @optional double timestamp;
 *       @optional string test;
 *   };
 *
 * Vanetza inbound CAM endpoints:
 *   topic='vanetza/in/cam'     type='JSONMessage'       RELIABLE + VOLATILE
 *   topic='vanetza/in/cam_enc' type='EncodedITSMessage' RELIABLE + VOLATILE
 *
 * Usage:
 *   ./dds_publisher           -> JSON mode  (default)
 *   ./dds_publisher enc       -> Encoded ASN.1 mode
 *   ./dds_publisher json 500  -> JSON mode, 500 ms interval
 *
 * Build: compile alongside JSONMessage.cxx, EncodedITSMessage.cxx and
 *        their generated PubSubTypes / CdrAux files.
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

// ── fastddsgen-generated ─────────────────────────────────────────────
#include "JSONMessage.h"
#include "JSONMessagePubSubTypes.h"
#include "EncodedITSMessage.h"
#include "EncodedITSMessagePubSubTypes.h"
// ─────────────────────────────────────────────────────────────────────

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <random>
#include <ctime>

using namespace eprosima::fastdds::dds;

#define STATION_ID 30

// ════════════════════════════════════════════════════════════════════
//  Utility helpers
// ════════════════════════════════════════════════════════════════════

static uint32_t now_epoch_seconds()
{
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

static double now_epoch_double()
{
    return std::chrono::duration<double>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

static uint32_t make_uuid()
{
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint32_t> dist;
    return dist(rng);
}

// ════════════════════════════════════════════════════════════════════
//  JSON CAM/CPM builder  (placed in JSONMessage::message)
// ════════════════════════════════════════════════════════════════════

static std::string build_cam_json(double lat, double lon,
                                  float speed_kmh,
                                  float heading = 153.0f,
                                  float yaw_rate = 11.0f,
                                  float long_accel = -0.4f)
{
    const uint64_t gen_delta =
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()) %
        65536ULL;

    std::ostringstream o;
    o << std::fixed;

    o << "{"
      << "\"generationDeltaTime\":" << gen_delta << ","
      << "\"camParameters\":{"

      << "\"basicContainer\":{"
      << "\"stationType\":5,"
      << "\"referencePosition\":{"
      << "\"latitude\":" << std::setprecision(7) << lat << ","
      << "\"longitude\":" << std::setprecision(7) << lon << ","
      << "\"positionConfidenceEllipse\":{"
      << "\"semiMajorAxisLength\":4095,"
      << "\"semiMinorAxisLength\":4095,"
      << "\"semiMajorAxisOrientation\":3601"
      << "},"
      << "\"altitude\":{"
      << "\"altitudeValue\":800001,"
      << "\"altitudeConfidence\":15"
      << "}"
      << "}"
      << "},"

      << "\"highFrequencyContainer\":{"
      << "\"basicVehicleContainerHighFrequency\":{"
      << "\"heading\":{"
      << "\"headingValue\":" << std::setprecision(1) << heading << ","
      << "\"headingConfidence\":127"
      << "},"
      << "\"speed\":{"
      << "\"speedValue\":" << std::setprecision(1) << speed_kmh << ","
      << "\"speedConfidence\":127"
      << "},"
      << "\"driveDirection\":2,"
      << "\"vehicleLength\":{"
      << "\"vehicleLengthValue\":1023,"
      << "\"vehicleLengthConfidenceIndication\":4"
      << "},"
      << "\"vehicleWidth\":62,"
      << "\"longitudinalAcceleration\":{"
      << "\"value\":" << std::setprecision(1) << long_accel << ","
      << "\"confidence\":102"
      << "},"
      << "\"curvature\":{"
      << "\"curvatureValue\":1023,"
      << "\"curvatureConfidence\":7"
      << "},"
      << "\"curvatureCalculationMode\":2,"
      << "\"yawRate\":{"
      << "\"yawRateValue\":" << std::setprecision(1) << yaw_rate << ","
      << "\"yawRateConfidence\":8"
      << "},"
      << "\"accelerationControl\":{"
      << "\"brakePedalEngaged\":false,"
      << "\"gasPedalEngaged\":false,"
      << "\"emergencyBrakeEngaged\":false,"
      << "\"collisionWarningEngaged\":false,"
      << "\"accEngaged\":false,"
      << "\"cruiseControlEngaged\":false,"
      << "\"speedLimiterEngaged\":false"
      << "},"
      << "\"steeringWheelAngle\":{"
      << "\"steeringWheelAngleValue\":512,"
      << "\"steeringWheelAngleConfidence\":127"
      << "}"
      << "}"
      << "},"

      << "\"lowFrequencyContainer\":{"
      << "\"basicVehicleContainerLowFrequency\":{"
      << "\"vehicleRole\":0,"
      << "\"exteriorLights\":{"
      << "\"lowBeamHeadlightsOn\":false,"
      << "\"highBeamHeadlightsOn\":false,"
      << "\"leftTurnSignalOn\":false,"
      << "\"rightTurnSignalOn\":false,"
      << "\"daytimeRunningLightsOn\":false,"
      << "\"reverseLightOn\":false,"
      << "\"fogLightOn\":false,"
      << "\"parkingLightsOn\":false"
      << "},"
      << "\"pathHistory\":[]"
      << "}"
      << "}"

      << "}" // camParameters
      << "}";

    return o.str();
}

static std::string build_cpm_json(double lat, double lon, double alt = 2.9) {
    const uint64_t ref_time =
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()); 
        //         %
        // 65536ULL;

    std::ostringstream o;
    o << std::fixed;

    o << "{"
      << "\"managementContainer\":{"
        << "\"referenceTime\":" << ref_time << ","
        << "\"referencePosition\":{"
          << "\"altitude\":{"
            << "\"altitudeConfidence\":15,"
            << "\"altitudeValue\":" << std::setprecision(1) << alt
          << "},"
          << "\"latitude\":" << std::setprecision(7) << lat << ","
          << "\"longitude\":" << std::setprecision(7) << lon << ","
          << "\"positionConfidenceEllipse\":{"
            << "\"semiMajorConfidence\":4095,"
            << "\"semiMajorOrientation\":0,"
            << "\"semiMinorConfidence\":4095"
          << "}"
        << "}"
      << "},"
      << "\"cpmContainers\":["
        // Originating Vehicle Container (ID 1)
        << "{"
          << "\"containerId\":1,"
          << "\"containerData\":{"
            << "\"orientationAngle\":{\"value\":0,\"confidence\":1}"
          << "}"
        << "},"
        // Sensor Information Container (ID 3)
        << "{"
          << "\"containerId\":3,"
          << "\"containerData\":["
            << "{\"sensorId\":1,\"sensorType\":1,\"perceptionRegionShape\":{\"radial\":{\"range\":100,\"horizontalOpeningAngleStart\":3601,\"horizontalOpeningAngleEnd\":3601}},\"shadowingApplies\":false},"
            << "{\"sensorId\":2,\"sensorType\":12,\"perceptionRegionShape\":{\"radial\":{\"range\":100,\"horizontalOpeningAngleStart\":3601,\"horizontalOpeningAngleEnd\":3601}},\"shadowingApplies\":false},"
            << "{\"sensorId\":3,\"sensorType\":10,\"perceptionRegionShape\":{\"radial\":{\"range\":100,\"horizontalOpeningAngleStart\":3601,\"horizontalOpeningAngleEnd\":3601}},\"shadowingApplies\":false},"
            << "{\"sensorId\":4,\"sensorType\":11,\"perceptionRegionShape\":{\"radial\":{\"range\":100,\"horizontalOpeningAngleStart\":3601,\"horizontalOpeningAngleEnd\":3601}},\"shadowingApplies\":false}"
          << "]"
        << "},"
        // Perceived Object Container (ID 5)
        << "{"
          << "\"containerId\":5,"
          << "\"containerData\":{"
            << "\"numberOfPerceivedObjects\":2,"
            << "\"perceivedObjects\":["
              // Object 1
              << "{"
                << "\"objectId\":115,"
                << "\"sensorIdList\":[1],"
                << "\"measurementDeltaTime\":19,"
                << "\"position\":{"
                  << "\"xCoordinate\":{\"value\":39.676389372854594,\"confidence\":1},"
                  << "\"yCoordinate\":{\"value\":-50.90099791468448,\"confidence\":1}"
                << "},"
                << "\"velocity\":{\"cartesianVelocity\":{"
                  << "\"xVelocity\":{\"value\":10.135577896110853,\"confidence\":1},"
                  << "\"yVelocity\":{\"value\":-4.274349156523026,\"confidence\":1}"
                << "}},"
                << "\"acceleration\":{\"cartesianAcceleration\":{"
                  << "\"xAcceleration\":{\"value\":0,\"confidence\":1},"
                  << "\"yAcceleration\":{\"value\":0,\"confidence\":1}"
                << "}},"
                << "\"angles\":{\"zAngle\":{\"value\":337.1340026855469,\"confidence\":1}},"
                << "\"classification\":[{\"objectClass\":{\"vehicleSubClass\":4},\"confidence\":71}],"
                << "\"objectDimensionX\":{\"value\":2.5999999046325684,\"confidence\":1}"
              << "},"
              // Object 2
              << "{"
                << "\"objectId\":120,"
                << "\"sensorIdList\":[3],"
                << "\"measurementDeltaTime\":19,"
                << "\"position\":{"
                  << "\"xCoordinate\":{\"value\":33.676389372854594,\"confidence\":1},"
                  << "\"yCoordinate\":{\"value\":-52.90099791468448,\"confidence\":1}"
                << "},"
                << "\"velocity\":{\"cartesianVelocity\":{"
                  << "\"xVelocity\":{\"value\":9.135577896110853,\"confidence\":1},"
                  << "\"yVelocity\":{\"value\":-2.274349156523026,\"confidence\":1}"
                << "}},"
                << "\"acceleration\":{\"cartesianAcceleration\":{"
                  << "\"xAcceleration\":{\"value\":0,\"confidence\":1},"
                  << "\"yAcceleration\":{\"value\":0,\"confidence\":1}"
                << "}},"
                << "\"angles\":{\"zAngle\":{\"value\":178.1340026855469,\"confidence\":1}},"
                << "\"classification\":[{\"objectClass\":{\"vehicleSubClass\":5},\"confidence\":71}],"
                << "\"objectDimensionX\":{\"value\":2.5999999046325684,\"confidence\":1}"
              << "}"
            << "]"
          << "}"
        << "}"
      << "]" // cpmContainers
      << "}";

    return o.str();
}

// ════════════════════════════════════════════════════════════════════
//  UPER-encoded CAM/CPM builder - using wireshark raw hex dump
// ════════════════════════════════════════════════════════════════════

static std::vector<uint8_t> make_encoded_cam_uper()
{
    // ── Live generationDeltaTime (patched into body bytes [0..1]) ──
    const uint16_t delta = static_cast<uint16_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() %
        65536ULL);

    // ── ITS PDU Header — 6 bytes, byte-aligned (ETSI EN 302 637-2) ─
    std::vector<uint8_t> msg;
    msg.reserve(46);
    unsigned char cam_dump[] = {0x2, 0x2, 0x0, 0x0, 0x0, 0x1, 0xd8, 0xdb, 0x40, 0x5a, 0x9e, 0x5f, 0x90, 0x8e, 0x68, 0xed, 0x7a, 0x9f, 0xff, 0xff, 0xfc, 0x23, 0xb7, 0x74, 0x3e, 0x50, 0x5f, 0xaf, 0xc8, 0xca, 0x7e, 0xbf, 0xe9, 0xea, 0x73, 0x37, 0xfe, 0xea, 0x11, 0x2a, 0x0, 0x6a, 0x9f, 0x80, 0x0, 0x0};
    msg.insert(msg.end(), cam_dump, cam_dump + 46);
    // msg.push_back(0x02);                                            // protocolVersion = 2
    // msg.push_back(0x02);                                            // messageID = 2 (CAM)
    // msg.push_back(static_cast<uint8_t>((STATION_ID >> 24) & 0xFF));
    // msg.push_back(static_cast<uint8_t>((STATION_ID >> 16) & 0xFF));
    // msg.push_back(static_cast<uint8_t>((STATION_ID >>  8) & 0xFF));
    // msg.push_back(static_cast<uint8_t>( STATION_ID        & 0xFF));

    // // ── UPER-encoded CAM body (33 bytes from Wireshark Data field) ─
    // // bytes [0..1] = generationDeltaTime, patched below
    // // bytes [2..32] = remainder (position, HFC, LFC) unchanged
    // const uint8_t body[33] = {
    //     static_cast<uint8_t>(delta >> 8),   // [0] generationDeltaTime MSB
    //     static_cast<uint8_t>(delta & 0xFF), // [1] generationDeltaTime LSB
    //     0x2, 0x2, 0x0, 0x0, 0x0, 0x1, 0xd8, 0xdb, 0x40, 0x5a, 0x9e, 0x5f, 0x90, 0x8e, 0x68, 0xed, 0x7a, 0x9f, 0xff, 0xff, 0xfc, 0x23, 0xb7, 0x74, 0x3e, 0x50, 0x5f, 0xaf, 0xc8, 0xca, 0x7e, 0xbf, 0xe9, 0xea, 0x73, 0x37, 0xfe, 0xea, 0x11, 0x2a, 0x0, 0x6a, 0x9f, 0x80, 0x0, 0x0
    // };
    //
    // msg.insert(msg.end(), body, body + 33);

    return msg; // WRONG total 39 bytes: 6-byte ITS header + 33-byte UPER body
}

static std::vector<uint8_t> make_encoded_cpm_uper()
{
    // ── Live generationDeltaTime (patched into body bytes [0..1]) ──
    const uint16_t delta = static_cast<uint16_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() %
        65536ULL);

    std::vector<uint8_t> msg;
    msg.reserve(131);
    unsigned char cpm_dump[] = {0x2, 0xe, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x9e, 0x99, 0x5a, 0x22, 0xf7, 0xca, 0x45, 0x57, 0xff, 0xff, 0xf8, 0x0, 0xc, 0x3e, 0x17, 0xa0, 0x3, 0x0, 0x0, 0x0, 0x21, 0xf0, 0x34, 0x2, 0x14, 0x0, 0xc9, 0xc2, 0x3c, 0x22, 0x40, 0x4c, 0x40, 0xc, 0x9c, 0x23, 0xc2, 0x24, 0x6, 0xa4, 0x0, 0xc9, 0xc2, 0x3c, 0x22, 0x40, 0x8b, 0x40, 0xc, 0x9c, 0x23, 0xc2, 0x24, 0x41, 0x0, 0x80, 0x9e, 0x13, 0x0, 0x39, 0xc0, 0x9a, 0xf, 0x7f, 0x6, 0x37, 0xb0, 0x78, 0x18, 0xe8, 0x7e, 0x98, 0xdf, 0x2a, 0x63, 0x94, 0x2, 0x94, 0x2, 0x8d, 0x2b, 0x12, 0x30, 0x0, 0x0, 0x10, 0x12, 0x33, 0xc2, 0x60, 0x7, 0x88, 0x13, 0x41, 0xa4, 0xe0, 0xc6, 0xf5, 0xab, 0x3, 0x1d, 0xe, 0x43, 0x1b, 0xf1, 0xcc, 0x72, 0x80, 0x52, 0x80, 0x50, 0xde, 0xa2, 0x46, 0x0, 0x0, 0x6, 0x2, 0xc6};
    msg.insert(msg.end(), cpm_dump, cpm_dump + 131);

    return msg; // total 131 bytes
}

// ════════════════════════════════════════════════════════════════════
//  DataWriter listener
// ════════════════════════════════════════════════════════════════════

class MyWriterListener : public DataWriterListener
{
public:
    std::atomic<int> matched{0};

    void on_publication_matched(DataWriter *, const PublicationMatchedStatus &info) override
    {
        matched += info.current_count_change;
        std::cout << (matched > 0
                          ? "[PUB] *** MATCHED with Vanetza reader! ***"
                          : "[PUB] Unmatched.")
                  << "  count=" << matched << std::endl;
    }
};

// ════════════════════════════════════════════════════════════════════
//  Main
// ════════════════════════════════════════════════════════════════════

std::atomic<bool> running{true};
void signal_handler(int)
{
    std::cout << "\n[PUB] Shutting down...\n";
    running = false;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // ── CLI ────────────────────────────────────────────────────────
    std::string msg_type = argc > 1 ? argv[1] : "cam";
    bool use_enc = (argc > 2 && std::string(argv[2]) == "enc");
    int interval_ms = 1000;
    if (argc > 3)
        interval_ms = std::stoi(argv[3]);

    std::string topic_name;
    if (msg_type == "cam")
    {
        topic_name = use_enc ? "vanetza/in/cam_enc" : "vanetza/in/cam";
    }
    else if (msg_type == "cpm")
    {
        topic_name = use_enc ? "vanetza/in/cpm_enc" : "vanetza/in/cpm";
    }

    const std::string TYPE_NAME = use_enc ? "EncodedITSMessage" : "JSONMessage";
    const int DOMAIN_ID = 0;

    std::cout << "=== Vanetza DDS CAM Publisher ===" << "\n"
              << "Mode:     " << (use_enc ? "enc (EncodedITSMessage)" : "json (JSONMessage)") << "\n"
              << "Topic:    " << topic_name << "\n"
              << "Type:     " << TYPE_NAME << "\n"
              << "QoS:      RELIABLE + VOLATILE" << "\n"
              << "Domain:   " << DOMAIN_ID << "\n"
              << "Interval: " << interval_ms << " ms" << "\n"
              << "=================================" << std::endl;

    // ── 1. Participant ─────────────────────────────────────────────
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("VanetzaDDSPublisher");
    DomainParticipant *participant =
        DomainParticipantFactory::get_instance()->create_participant(DOMAIN_ID, pqos);
    if (!participant)
    {
        std::cerr << "[PUB] ERROR: create_participant\n";
        return 1;
    }

    // ── 2. Register type (use generated PubSubType) ────────────────
    TypeSupport type_support(use_enc
                                 ? static_cast<TopicDataType *>(new EncodedITSMessagePubSubType())
                                 : static_cast<TopicDataType *>(new JSONMessagePubSubType()));
    type_support.register_type(participant);

    // ── 3. Topic ───────────────────────────────────────────────────
    Topic *topic = participant->create_topic(topic_name, TYPE_NAME, TOPIC_QOS_DEFAULT);
    if (!topic)
    {
        std::cerr << "[PUB] ERROR: create_topic\n";
        return 1;
    }

    // ── 4. Publisher ───────────────────────────────────────────────
    Publisher *publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    if (!publisher)
    {
        std::cerr << "[PUB] ERROR: create_publisher\n";
        return 1;
    }

    // ── 5. DataWriter — RELIABLE + VOLATILE ───────────────────────
    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    wqos.durability().kind = VOLATILE_DURABILITY_QOS;

    MyWriterListener writer_listener;
    DataWriter *writer = publisher->create_datawriter(topic, wqos, &writer_listener);
    if (!writer)
    {
        std::cerr << "[PUB] ERROR: create_datawriter\n";
        return 1;
    }

    std::cout << "[PUB] Waiting for Vanetza reader match..." << std::endl;

    // ── Publish loop ───────────────────────────────────────────────
    uint32_t seq = 0;
    double lat = 52.5200;
    double lon = 13.4050;
    float speed = 50.0f;

    while (running)
    {
        bool ok = false;

        if (use_enc)
        {
            // ── Encoded ASN.1 / UPER mode ─────────────────────────
            EncodedITSMessage msg;

            // Mandatory fields
            msg.topic(topic_name); // string topic
            if (msg_type == "cam")
            {
                msg.message(make_encoded_cam_uper()); // sequence<octet> message
            }
            else if (msg_type == "cpm")
            {
                msg.message(make_encoded_cpm_uper()); // sequence<octet> message
            }

            // Optional metadata
            msg.stationID(static_cast<int32_t>(STATION_ID));
            msg.timestamp(now_epoch_double());
            msg.packetSize(static_cast<int16_t>(msg.message().size()));
            msg.newInfo(true);
            // rssi / receiverID / receiverType / test left absent (@optional)

            ok = writer->write(&msg);
            if (ok)
            {
                std::cout << "[PUB] Sent EncodedITSMessage #" << seq
                          << " | topic=" << msg.topic()
                          << " | bytes=" << msg.message().size()
                          << " | stationID=" << msg.stationID().value()
                          << " | timestamp=" << std::fixed << std::setprecision(3)
                          << msg.timestamp().value()
                          << " | matched=" << writer_listener.matched.load()
                          << "\n  hex: ";
                const size_t show = std::min(msg.message().size(), (size_t)16);
                for (size_t i = 0; i < show; ++i)
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(msg.message()[i]) << ' ';
                if (msg.message().size() > 16)
                    std::cout << "...";
                std::cout << std::dec << std::endl;
            }
        }
        else
        {
            // ── JSON mode ─────────────────────────────────────────
            JSONMessage msg;
            msg.uuid(make_uuid());
            msg.datetime(now_epoch_seconds());
            msg.topic(topic_name);
            if (msg_type == "cam")
            {
                msg.message(build_cam_json(lat, lon, speed));
            }
            else if (msg_type == "cpm")
            {
                msg.message(build_cpm_json(lat, lon, speed));
            }
            ok = writer->write(&msg);
            if (ok)
            {
                std::cout << "[PUB] Sent JSONMessage #" << seq
                          << " | uuid=" << msg.uuid()
                          << " | datetime=" << msg.datetime()
                          << " | topic=" << msg.topic()
                          << " | matched=" << writer_listener.matched.load()
                          << "\n  message=" << msg.message()
                          << std::endl;
            }
        }
        if (!ok)
            std::cerr << "[PUB] write() failed for #" << seq << std::endl;

        lat += 0.00005;
        lon += 0.00005;
        speed = 45.0f + static_cast<float>(seq % 20);
        ++seq;

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    // ── Cleanup ────────────────────────────────────────────────────
    publisher->delete_datawriter(writer);
    participant->delete_publisher(publisher);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);
    return 0;
}