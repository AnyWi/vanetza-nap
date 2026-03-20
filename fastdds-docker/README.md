# Vanetza / FastDDS Publish-Subscribe Test

A complete, ready-to-run test suite for DDS pub/sub using **Vanetza** and **FastDDS**.

## Project Structure

```
vanetza-dds-test/
├── src/
│   ├── publisher.cpp              # FastDDS publisher (generic messages)
│   ├── subscriber.cpp             # FastDDS subscriber (with latency stats)
│   ├── vanetza_cam_publisher.cpp  # Vanetza CAM encoder + DDS publisher
│   └── vanetza_cam_subscriber.cpp # DDS subscriber + Vanetza CAM decoder
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── build_native.sh
├── run.sh
└── README.md
```

## What Each Test Does

| Test | Description |
|------|-------------|
| `dds_publisher` + `dds_subscriber` | Basic FastDDS pub/sub with JSON-like CAM-shaped payloads. No Vanetza required. |
| `vanetza_cam_publisher` + `vanetza_cam_subscriber` | Full Vanetza integration: builds real ASN.1/UPER-encoded CAM messages, publishes over DDS, decodes on receive. |

---

## Option 1: Docker (Recommended, zero setup)

```bash
# Build and start both publisher and subscriber
docker-compose up --build

# Or run them separately in 2 terminals:
docker build -t vanetza-dds .

# Terminal 1 - Subscriber
docker run --rm --network host vanetza-dds /app/run.sh subscriber

# Terminal 2 - Publisher
docker run --rm --network host vanetza-dds /app/run.sh publisher
```

---

## Option 2: Native Build (Ubuntu 22.04)

```bash
chmod +x build_native.sh
./build_native.sh
```

Then in two terminals:

```bash
# Terminal 1
./build/dds_subscriber

# Terminal 2
./build/dds_publisher 1000   # publish every 1000ms
```

---

## Option 3: Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Expected Output

### Subscriber terminal:
```
=== Vanetza/FastDDS Subscriber ===
Topic:  VanetzaTestTopic
Domain: 0
==================================
[SUB] Waiting for messages...
[SUB] Publisher matched!
[SUB] Received #0 | latency=2ms | total=1
      Payload: {"messageType":"CAM","stationId":12345,"sequence":0,"position":{"lat":52.520000,"lon":13.405000},"speed":30.00,...}
[SUB] Received #1 | latency=1ms | total=2
      Payload: {"messageType":"CAM","stationId":12345,"sequence":1,...}
```

### Publisher terminal:
```
=== Vanetza/FastDDS Publisher ===
Topic:    VanetzaTestTopic
Domain:   0
Interval: 1000 ms
=================================
[PUB] Ready. Waiting for subscribers...
[PUB] Sent #0 | matched=1 | payload={"messageType":"CAM",...}
[PUB] Sent #1 | matched=1 | payload={"messageType":"CAM",...}
```

---

## DDS Configuration

| Parameter | Value |
|-----------|-------|
| Domain ID | 0 |
| Topic | `VanetzaTestTopic` / `VanetzaCAMTopic` |
| QoS Reliability | RELIABLE |
| QoS Durability | TRANSIENT_LOCAL |
| Transport | UDP Multicast (FastDDS default) |
| CAM rate | 1 Hz (configurable) |

---

## Customization

**Change publish rate:**
```bash
./build/dds_publisher 500   # 500ms = 2 Hz
./build/dds_publisher 100   # 100ms = 10 Hz
```

**Limit number of messages:**
```bash
./build/dds_publisher 1000 50   # publish 50 messages at 1 Hz then stop
```

**Change DDS domain ID** (edit source or add CLI arg):
```cpp
// In publisher.cpp / subscriber.cpp, line ~100
const int DOMAIN_ID = 0;  // change this
```

---

## FastDDS XML Profile (optional)

You can tune FastDDS further with a profile XML. Set environment variable:
```bash
export FASTRTPS_DEFAULT_PROFILES_FILE=dds_profile.xml
```

Example `dds_profile.xml`:
```xml
<?xml version="1.0" encoding="UTF-8" ?>
<profiles>
  <transport_descriptors>
    <transport_descriptor>
      <transport_id>udp_transport</transport_id>
      <type>UDPv4</type>
    </transport_descriptor>
  </transport_descriptors>
</profiles>
```

---

## Troubleshooting

**Publisher and subscriber don't see each other:**
- Ensure both use the same domain ID (default: 0)
- Check firewall allows UDP multicast (239.255.0.0/8)
- On separate machines: ensure they're on the same subnet

**FastDDS not found by CMake:**
```bash
export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH
```

**Vanetza not found:**
- The basic `dds_publisher`/`dds_subscriber` work without Vanetza
- Only `vanetza_cam_*` require Vanetza to be installed
