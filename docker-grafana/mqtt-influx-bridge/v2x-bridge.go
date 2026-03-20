package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	influxdb2 "github.com/influxdata/influxdb-client-go/v2"
	"github.com/influxdata/influxdb-client-go/v2/api"
)

// --- Configuration ---

type Config struct {
	MQTTBrokers  []string
	InfluxURL    string
	InfluxToken  string
	InfluxOrg    string
	InfluxBucket string
}

var cfg = Config{
	MQTTBrokers:  []string{"tcp://192.168.98.10:1883", "tcp://192.168.98.20:1883"},
	InfluxURL:    "http://localhost:8086",
	InfluxToken:  "token-xyz",
	InfluxOrg:    "anytry",
	InfluxBucket: "main_bucket",
}

// Topics to subscribe (# is wildcard for the message type suffix)
var subscribeTopics = []string{
	"vanetza/out/#",
	"vanetza/time/#",
}

// --- Payload structures ---

type TestFields struct {
	StartProcessingTimestamp float64 `json:"start_processing_timestamp"`
	JSONTimestamp            float64 `json:"json_timestamp"`
	WaveTimestamp            float64 `json:"wave_timestamp"`
}

// VanetzaMessage covers both vanetza/out and vanetza/time payloads.
// Unknown/extra fields land in Fields.
type VanetzaMessage struct {
	Timestamp    float64                `json:"timestamp"`
	StationID    int64                  `json:"stationID"`
	StationAddr  string                 `json:"stationAddr"`
	ReceiverID   int64                  `json:"receiverID"`
	ReceiverType int64                  `json:"receiverType"`
	NewInfo      *bool                  `json:"newInfo,omitempty"`
	RSSI         *int64                 `json:"rssi,omitempty"`
	PacketSize   *int64                 `json:"packet_size,omitempty"`
	Fields       map[string]interface{} `json:"fields"`
	Test         TestFields             `json:"test"`
}

// --- MQTT message handler ---

func makeMessageHandler(brokerAddr string, writeAPI api.WriteAPIBlocking) mqtt.MessageHandler {
	return func(client mqtt.Client, msg mqtt.Message) {
		topic := msg.Topic()
		payload := msg.Payload()

		// Determine message category and type suffix
		// e.g. "vanetza/out/cam" -> category="out", msgType="cam"
		parts := strings.SplitN(topic, "/", 3)
		if len(parts) < 3 {
			log.Printf("[WARN] unexpected topic format: %s", topic)
			return
		}
		category := parts[1] // "out" or "time"
		msgType := parts[2]  // e.g. "cam", "denm", etc.

		var vmsg VanetzaMessage
		if err := json.Unmarshal(payload, &vmsg); err != nil {
			log.Printf("[WARN] failed to parse JSON on topic %s: %v", topic, err)
			return
		}

		// Convert unix float timestamp to time.Time (nanosecond precision)
		sec := int64(vmsg.Timestamp)
		nsec := int64((vmsg.Timestamp - float64(sec)) * 1e9)
		msgTime := time.Unix(sec, nsec)

		// Measurement name: vanetza_out or vanetza_time
		measurement := "vanetza_" + category

		// Tags
		tags := map[string]string{
			"broker":      brokerAddr,
			"topic":       topic,
			"msg_type":    msgType,
			"station_addr": vmsg.StationAddr,
		}

		// Fields
		fields := map[string]interface{}{
			"station_id":    vmsg.StationID,
			"receiver_id":   vmsg.ReceiverID,
			"receiver_type": vmsg.ReceiverType,
			"timestamp":     vmsg.Timestamp,
		}

		if vmsg.NewInfo != nil {
			fields["new_info"] = *vmsg.NewInfo
		}
		if vmsg.RSSI != nil {
			fields["rssi"] = *vmsg.RSSI
		}
		if vmsg.PacketSize != nil {
			fields["packet_size"] = *vmsg.PacketSize
		}

		// Test sub-fields
		switch category {
		case "out":
			if vmsg.Test.StartProcessingTimestamp > 0 {
				fields["test_start_processing_ts"] = vmsg.Test.StartProcessingTimestamp
				fields["latency_decode_ms"] = (vmsg.Test.JSONTimestamp - vmsg.Timestamp) * 1000
			}
			if vmsg.Test.JSONTimestamp > 0 {
				fields["test_json_ts"] = vmsg.Test.JSONTimestamp
			}
		case "time":
			if vmsg.Test.WaveTimestamp > 0 {
				fields["test_wave_ts"] = vmsg.Test.WaveTimestamp
				fields["latency_encode_ms"] = (vmsg.Test.WaveTimestamp - vmsg.Timestamp) * 1000
			}
			if vmsg.Test.StartProcessingTimestamp > 0 {
				fields["test_start_processing_ts"] = vmsg.Test.StartProcessingTimestamp
			}
		}

		// Include any extra decoded fields from the "fields" object
		for k, v := range vmsg.Fields {
			fields["field_"+k] = v
		}

		// Write to InfluxDB
		p := influxdb2.NewPoint(measurement, tags, fields, msgTime)
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := writeAPI.WritePoint(ctx, p); err != nil {
			log.Printf("[ERROR] influx write failed for topic %s: %v", topic, err)
		} else {
			log.Printf("[INFO] wrote %s | broker=%s station=%d type=%s ts=%.3f",
				measurement, brokerAddr, vmsg.StationID, msgType, vmsg.Timestamp)
		}
	}
}

// --- Connect to a single MQTT broker ---

func connectBroker(brokerURL string, handler mqtt.MessageHandler) mqtt.Client {
	// Use broker address as part of client ID to keep them unique
	clientID := fmt.Sprintf("v2x-bridge-%s-%d", strings.ReplaceAll(brokerURL, ":", "-"), time.Now().UnixNano())

	opts := mqtt.NewClientOptions().
		AddBroker(brokerURL).
		SetClientID(clientID).
		SetCleanSession(true).
		SetAutoReconnect(true).
		SetConnectRetryInterval(5 * time.Second).
		SetOnConnectHandler(func(c mqtt.Client) {
			log.Printf("[INFO] connected to broker %s, subscribing...", brokerURL)
			for _, topic := range subscribeTopics {
				if token := c.Subscribe(topic, 1, handler); token.Wait() && token.Error() != nil {
					log.Printf("[ERROR] subscribe %s on %s: %v", topic, brokerURL, token.Error())
				} else {
					log.Printf("[INFO] subscribed to %s on %s", topic, brokerURL)
				}
			}
		}).
		SetConnectionLostHandler(func(c mqtt.Client, err error) {
			log.Printf("[WARN] lost connection to %s: %v — will reconnect", brokerURL, err)
		})

	client := mqtt.NewClient(opts)
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		log.Fatalf("[FATAL] cannot connect to %s: %v", brokerURL, token.Error())
	}
	return client
}

// --- Main ---

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
	log.Println("[INFO] Starting V2X MQTT→InfluxDB bridge")

	// Override config from environment variables if set
	if v := os.Getenv("INFLUX_URL"); v != "" {
		cfg.InfluxURL = v
	}
	if v := os.Getenv("INFLUX_TOKEN"); v != "" {
		cfg.InfluxToken = v
	}
	if v := os.Getenv("INFLUX_ORG"); v != "" {
		cfg.InfluxOrg = v
	}
	if v := os.Getenv("INFLUX_BUCKET"); v != "" {
		cfg.InfluxBucket = v
	}

	// InfluxDB client (shared across all MQTT connections)
	influxClient := influxdb2.NewClientWithOptions(
		cfg.InfluxURL,
		cfg.InfluxToken,
		influxdb2.DefaultOptions().
			SetBatchSize(50).
			SetFlushInterval(1000),
	)
	defer influxClient.Close()

	writeAPI := influxClient.WriteAPIBlocking(cfg.InfluxOrg, cfg.InfluxBucket)

	// Connect to all brokers
	clients := make([]mqtt.Client, 0, len(cfg.MQTTBrokers))
	for _, broker := range cfg.MQTTBrokers {
		handler := makeMessageHandler(broker, writeAPI)
		c := connectBroker(broker, handler)
		clients = append(clients, c)
		log.Printf("[INFO] broker %s connected", broker)
	}

	// Wait for termination signal
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	<-sigCh

	log.Println("[INFO] shutting down...")
	for _, c := range clients {
		c.Disconnect(500)
	}
	log.Println("[INFO] done")
}