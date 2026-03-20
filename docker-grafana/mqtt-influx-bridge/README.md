# V2X MQTT to InfluxDB bridge(Go)
Simple bridge to push data from mqtt to InfluxDb

## Relevant fields
latency_encode_ms --> from message received on MQTT/DDS/ZENOH (-JSON) to sent on physical interface (-ASN1)

latency_decode_ms --> from message received on physical interface to 

## Build
go mod init v2x-bridge
then run to auto fetch dependencies
`go tidy`
or
```bash
go get github.com/eclipse/paho.mqtt.golang
go get github.com/influxdata/influxdb-client-go/v2
go get v2x-bridge
```

## Run
`./v2x-bridge`
or
`go run v2x-bridge`
