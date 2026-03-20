/**
 * Vanetza DDS Subscriber
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
 * Vanetza output topics:
 *   vanetza/out/cam      type='JSONMessage'       RELIABLE + TRANSIENT_LOCAL
 *   vanetza/out/cam_enc  type='EncodedITSMessage' RELIABLE + TRANSIENT_LOCAL
 *   vanetza/own/cam      type='JSONMessage'       (self-generated CAMs)
 *   vanetza/time/cam     type='JSONMessage'       (with timing info)
 *
 * Usage:
 *   ./dds_subscriber                        -> vanetza/out/cam  (JSON)
 *   ./dds_subscriber enc                    -> vanetza/out/cam_enc (encoded)
 *   ./dds_subscriber json vanetza/own/cam   -> custom JSON topic
 *
 * Build: compile alongside JSONMessage.cxx, EncodedITSMessage.cxx and
 *        their generated PubSubTypes / CdrAux files.
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

// ── fastddsgen-generated ─────────────────────────────────────────────
#include "JSONMessage.h"
#include "JSONMessagePubSubTypes.h"
#include "EncodedITSMessage.h"
#include "EncodedITSMessagePubSubTypes.h"
// ─────────────────────────────────────────────────────────────────────

#include <iostream>
#include <vector>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <iomanip>
#include <ctime>

using namespace eprosima::fastdds::dds;

// ════════════════════════════════════════════════════════════════════
//  JSON listener — prints all four IDL fields
// ════════════════════════════════════════════════════════════════════

class JSONListener : public DataReaderListener {
public:
    std::atomic<uint64_t> received{0};

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) override {
        std::cout << "[SUB] "
                  << (info.current_count_change > 0 ? "*** MATCHED ***" : "Unmatched")
                  << " (Vanetza writer)" << std::endl;
    }

    void on_data_available(DataReader* reader) override {
        JSONMessage msg;
        SampleInfo  info;
        while (reader->take_next_sample(&msg, &info) == ReturnCode_t::RETCODE_OK) {
            if (!info.valid_data) continue;
            ++received;
            std::cout << "[SUB] JSONMessage #" << received       << "\n"
                      << "  uuid     : " << msg.uuid()           << "\n"
                      << "  datetime : " << msg.datetime()
                      << " (" << format_epoch(msg.datetime()) << ")\n"
                      << "  topic    : " << msg.topic()          << "\n"
                      << "  message  : " << msg.message()        << "\n"
                      << std::endl;
        }
    }

private:
    static std::string format_epoch(uint32_t epoch_s) {
        std::time_t t = static_cast<std::time_t>(epoch_s);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        return buf;
    }
};

// ════════════════════════════════════════════════════════════════════
//  Encoded listener — prints all IDL fields including all @optional ones
// ════════════════════════════════════════════════════════════════════

class EncodedListener : public DataReaderListener {
public:
    std::atomic<uint64_t> received{0};

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) override {
        std::cout << "[SUB] "
                  << (info.current_count_change > 0 ? "*** MATCHED ***" : "Unmatched")
                  << " (Vanetza writer)" << std::endl;
    }

    void on_data_available(DataReader* reader) override {
        EncodedITSMessage msg;
        SampleInfo        info;
        while (reader->take_next_sample(&msg, &info) == ReturnCode_t::RETCODE_OK) {
            if (!info.valid_data) continue;
            ++received;

            std::cout << "[SUB] EncodedITSMessage #" << received   << "\n"
                      << "  topic      : " << msg.topic()          << "\n"
                      << "  bytes      : " << msg.message().size() << "\n";

            // Print hex dump of the UPER payload
            std::cout << "  hex        : ";
            const size_t show = std::min(msg.message().size(), (size_t)24);
            for (size_t i = 0; i < show; ++i)
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(msg.message()[i]) << ' ';
            if (msg.message().size() > 24) std::cout << "...";
            std::cout << std::dec << "\n";

            // Optional fields — only print if present
            if (msg.stationID().has_value())
                std::cout << "  stationID  : " << msg.stationID().value()  << "\n";
            if (msg.receiverID().has_value())
                std::cout << "  receiverID : " << msg.receiverID().value() << "\n";
            if (msg.receiverType().has_value())
                std::cout << "  receiverType: " << msg.receiverType().value() << "\n";
            if (msg.rssi().has_value())
                std::cout << "  rssi       : " << msg.rssi().value()       << " dBm\n";
            if (msg.packetSize().has_value())
                std::cout << "  packetSize : " << msg.packetSize().value() << "\n";
            if (msg.newInfo().has_value())
                std::cout << "  newInfo    : " << (msg.newInfo().value() ? "true" : "false") << "\n";
            if (msg.timestamp().has_value())
                std::cout << "  timestamp  : " << std::fixed << std::setprecision(3)
                          << msg.timestamp().value() << "\n";
            if (msg.test().has_value())
                std::cout << "  test       : " << msg.test().value()       << "\n";

            std::cout << std::endl;
        }
    }
};

// ════════════════════════════════════════════════════════════════════
//  Main
// ════════════════════════════════════════════════════════════════════

std::atomic<bool> running{true};
void signal_handler(int) { std::cout << "\n[SUB] Shutting down...\n"; running = false; }

int main(int argc, char* argv[])
{
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    // ── CLI ────────────────────────────────────────────────────────
    bool use_enc = (argc > 1 && std::string(argv[1]) == "enc");
    std::string TOPIC_NAME = use_enc ? "vanetza/out/cam_enc" : "vanetza/out/cam";
    if (argc > 2) TOPIC_NAME = argv[2];  // optional topic override

    const std::string TYPE_NAME = use_enc ? "EncodedITSMessage" : "JSONMessage";
    const int DOMAIN_ID = 0;

    std::cout << "=== Vanetza DDS Subscriber ===" << "\n"
              << "Mode:   " << (use_enc ? "enc (EncodedITSMessage)" : "json (JSONMessage)") << "\n"
              << "Topic:  " << TOPIC_NAME  << "\n"
              << "Type:   " << TYPE_NAME   << "\n"
              << "QoS:    RELIABLE + TRANSIENT_LOCAL" << "\n"
              << "===============================" << std::endl;

    // ── 1. Participant ─────────────────────────────────────────────
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("VanetzaCamSubscriber");
    DomainParticipant* participant =
        DomainParticipantFactory::get_instance()->create_participant(DOMAIN_ID, pqos);
    if (!participant) { std::cerr << "[SUB] ERROR: create_participant\n"; return 1; }

    // ── 2. Register type (use generated PubSubType) ────────────────
    TypeSupport type_support(use_enc
        ? static_cast<TopicDataType*>(new EncodedITSMessagePubSubType())
        : static_cast<TopicDataType*>(new JSONMessagePubSubType()));
    type_support.register_type(participant);

    // ── 3. Topic ───────────────────────────────────────────────────
    Topic* topic = participant->create_topic(TOPIC_NAME, TYPE_NAME, TOPIC_QOS_DEFAULT);
    if (!topic) { std::cerr << "[SUB] ERROR: create_topic\n"; return 1; }

    // ── 4. Subscriber ─────────────────────────────────────────────
    Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    if (!subscriber) { std::cerr << "[SUB] ERROR: create_subscriber\n"; return 1; }

    // ── 5. DataReader — RELIABLE + TRANSIENT_LOCAL ────────────────
    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.durability().kind  = TRANSIENT_LOCAL_DURABILITY_QOS;

    JSONListener    json_listener;
    EncodedListener enc_listener;

    DataReader* reader = use_enc
        ? subscriber->create_datareader(topic, rqos, &enc_listener)
        : subscriber->create_datareader(topic, rqos, &json_listener);
    if (!reader) { std::cerr << "[SUB] ERROR: create_datareader\n"; return 1; }

    std::cout << "[SUB] Waiting for messages from Vanetza..." << std::endl;

    while (running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const uint64_t total = use_enc
        ? enc_listener.received.load()
        : json_listener.received.load();
    std::cout << "[SUB] Total received: " << total << std::endl;

    // ── Cleanup ────────────────────────────────────────────────────
    subscriber->delete_datareader(reader);
    participant->delete_subscriber(subscriber);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);
    return 0;
}