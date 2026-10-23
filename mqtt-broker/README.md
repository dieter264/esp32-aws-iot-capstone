# Local MQTT TLS Test Broker

This folder contains a minimal local Mosquitto setup for testing the ESP32 MQTT publish path with TLS.

The general ESP32 setup, including Wi-Fi provisioning, AP access, sensor setup, SNTP setup, build and flash instructions, is described in the [top-level project README](../README.md).

This README only describes the local MQTT broker test setup.

## 1. Test setup

The MQTT test verifies the following path:

```text
BME280, DS18B20, or TL136 acquisition attempt
- Sensor status and error_status updated
- Sensor-specific data-ready event posted
- MQTT client task
- TLS connection to Mosquitto
- MQTT publish
- local subscriber receives JSON payload
```

The test setup uses:

```text
ESP32-S3 firmware
- Wi-Fi STA connection
- local Mosquitto broker on the development machine or VM
- TLS on port 8883
- locally generated test CA
- server certificate signed by the test CA
- no MQTT username/password
- no client certificate
```

This matches the firmware MQTT design:

```text
- MQTT over TLS only
- server certificate verification using configured CA certificate
- no MQTT username/password
- no client certificate authentication
```

The Mosquitto broker runs on the development machine or VM. 
**Note:** The ESP32 must be able to reach this machine over the local network.

## 2. Folder layout

Expected folder structure:

```text
mqtt-broker/
├── README.md
├── mosquitto.conf
└── certs/
    ├── .gitkeep
    ├── ca.crt
    ├── ca.key
    ├── ca.srl
    ├── server.crt
    ├── server.csr
    ├── server.ext
    └── server.key
```

**Note:** The certificate files are generated locally and must not be committed to Git.

## 3. Install Mosquitto tools

On Ubuntu:

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients openssl
```

Check that Mosquitto is available:

```bash
mosquitto -h
mosquitto_sub -h
openssl version
```

## 4. Create certificates

```bash
cd mqtt-broker
```
Run the following commands from the `mqtt-broker/` folder.

The `certs/` folder already exists in the repo structure.

Replace `<BROKER_IP>` with the IP address of the machine running Mosquitto. 

Example IP address:

```text
192.168.178.68
```

The ESP32 MQTT URI must later use the same IP address that is written into the server certificate `subjectAltName`. Otherwise TLS server certificate verification will fail.

### 4.1 Create local test CA

```bash
openssl req -x509 -newkey rsa:2048 -days 3650 -nodes \
  -keyout certs/ca.key \
  -out certs/ca.crt \
  -subj "/CN=ESP32 MQTT Test CA"
```

This creates:

```text
certs/ca.key
certs/ca.crt
```

The CA certificate `ca.crt` is later copied into the ESP32 MQTT CA certificate configuration field.

### 4.2 Create server key and certificate signing request

```bash
openssl req -newkey rsa:2048 -nodes \
  -keyout certs/server.key \
  -out certs/server.csr \
  -subj "/CN=<BROKER_IP>"
```
This creates:

```text
certs/server.key
certs/server.csr
```

### 4.3 Create server certificate extension file

The server certificate must contain a `subjectAltName` matching the broker address used by the ESP32.

For an IP-based MQTT URI, use:

```bash
cat > certs/server.ext <<EOF_EXT
subjectAltName = IP:<BROKER_IP>
EOF_EXT
```

This creates:

```text
certs/server.ext
```

### 4.4 Sign server certificate

```bash
openssl x509 -req -in certs/server.csr \
  -CA certs/ca.crt \
  -CAkey certs/ca.key \
  -CAcreateserial \
  -out certs/server.crt \
  -days 3650 \
  -extfile certs/server.ext
```

This creates:

```text
certs/server.crt
certs/ca.srl
```

The broker uses:

```text
certs/ca.crt
certs/server.crt
certs/server.key
```

The ESP32 uses only:

```text
certs/ca.crt
```

## 5. Start Mosquitto

The local broker configuration is stored in:

```text
mqtt-broker/mosquitto.conf
```

Expected minimal configuration:

```conf
listener 8883 0.0.0.0
protocol mqtt

cafile certs/ca.crt
certfile certs/server.crt
keyfile certs/server.key

require_certificate false
allow_anonymous true

log_type all
connection_messages true
```

Start Mosquitto from the `mqtt-broker/` folder:

```bash
mosquitto -c mosquitto.conf -v
```

Expected result:

```text
Mosquitto starts successfully
Mosquitto listens on port 8883
TLS certificate files are loaded
```

If Mosquitto cannot read the key file, check file permissions.

If the ESP32 cannot connect, check that the broker machine firewall allows incoming TCP connections on port `8883`.

Useful check:

```bash
ss -lntp | grep 8883
```

## 6. Start `mosquitto_sub`

Open a second terminal and run:

```bash
cd mqtt-broker
```

Subscribe to all topics:

```bash
mosquitto_sub \
  -h <BROKER_IP> \
  -p 8883 \
  --cafile certs/ca.crt \
  -t '#' \
  -v
```

## 7. ESP32 MQTT configuration

Configure MQTT via the ESP32 HTTPS configuration page.

The general access procedure for the ESP32 configuration page is described in the top-level README.

### Broker URI

Use:

```text
mqtts://<BROKER_IP>:8883
```

Example:

```text
mqtts://192.168.178.68:8883
```

The broker IP must match the `subjectAltName` in the generated server certificate.

If the certificate was generated with:

```text
subjectAltName = IP:192.168.178.68
```

then the ESP32 broker URI must use:

```text
mqtts://192.168.178.68:8883
```

Using another IP address or a hostname will cause TLS server certificate verification to fail.

### CA certificate

Copy the complete content of:

```text
mqtt-broker/certs/ca.crt
```

into the ESP32 MQTT CA certificate configuration field.

The full PEM block is required:

```text
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
```

Save the configuration and wait for the MQTT component to reload the configuration.

### Enable the sensors

With fresh NVS configuration and the default build settings, all sensors are disabled. On the HTTPS configuration page, enable each sensor connected to the board and save its settings. For the DS18B20, enter its matching 64-bit ROM address. For the TL136, set the R4CVA02 address and input channel to match the wiring.

Check the active-device state and sensor readings on the configuration page before expecting MQTT messages. Wiring and supply details are documented in [docs/HARDWARE.md](../docs/HARDWARE.md).

## Expected result

With a correct broker URI and CA certificate, the ESP32 should connect to Mosquitto via TLS and publish a separate JSON message for each enabled, running sensor after its data-ready events.

Expected firmware behavior:

```text
Wi-Fi STA connected
MQTT client starts
MQTT_EVENT_CONNECTED
Enabled sensors continue acquisition
Sensor-specific APP_EVENT_*_DATA_READY events are posted
MQTT publishes the corresponding sensor JSON
```

### Topics and payload units

| Sensor | Topic |
|---|---|
| BME280 | `devices/001-esp32s3/01-bme280` |
| DS18B20 | `devices/001-esp32s3/02-ds18b20` |
| TL136 | `devices/001-esp32s3/03-tl136` |

The device identifier and topics are currently fixed in the firmware. Messages use QoS 0 with the MQTT retain flag disabled. Start the subscriber before waiting for the next sensor event; the broker does not replay these messages to a new subscriber.

| Data field | Interpretation |
|---|---|
| `temp_degCx100` | Temperature in °C × 100; divide by 100 |
| `press_PaQ24d8` | BME280 pressure in Pa × 256; divide by 256 |
| `hum_RhQ22d10` | BME280 relative humidity in %RH × 1024; divide by 1024 |
| `current_mAx100` | TL136 loop current in mA × 100; divide by 100 |
| `water_level_mm` | TL136 water level in millimetres |

### Successful readings

Example subscriber output with all three sensors enabled, running, and successfully sampled:

```text
devices/001-esp32s3/01-bme280 {"data":{"temp_degCx100":2843,"press_PaQ24d8":25653085,"hum_RhQ22d10":54210},"sample_time_valid":true,"sample_time_utc_us":1782152669572637,"error_status":"ESP_OK"}
devices/001-esp32s3/02-ds18b20 {"data":{"temp_degCx100":2350},"sample_time_valid":true,"sample_time_utc_us":1782152669572637,"error_status":"ESP_OK"}
devices/001-esp32s3/03-tl136 {"data":{"current_mAx100":1200,"water_level_mm":1000},"sample_time_valid":true,"sample_time_utc_us":1782152669572637,"error_status":"ESP_OK"}
```

The exact values and timestamps depend on each sensor's sampling time and SNTP state. The TL136 example assumes a 2000 mm full-scale range.

### Timestamp validity

Before the first SNTP synchronization, a successful BME280 reading may look like this:

```json
{
  "data": {
    "temp_degCx100": 2843,
    "press_PaQ24d8": 25653085,
    "hum_RhQ22d10": 54210
  },
  "sample_time_valid": false,
  "sample_time_utc_us": 0,
  "error_status": "ESP_OK"
}
```

After SNTP synchronization, successful readings include a valid UTC timestamp in microseconds, as in the subscriber examples above. Sensor-read success (`error_status`) and UTC timestamp validity (`sample_time_valid`) are independent.

### Failed readings

Each acquisition attempt updates `error_status`. When a read fails, the running sensor keeps the data and timestamps from its last successful sample and still posts its data-ready event. A connected MQTT client can therefore report the failure alongside the previous sample:

```json
{
  "data": {
    "current_mAx100": 1200,
    "water_level_mm": 1000
  },
  "sample_time_valid": true,
  "sample_time_utc_us": 1782152669572637,
  "error_status": "ESP_ERR_TIMEOUT"
}
```

This TL136 example describes a timeout after an earlier successful sample. The unchanged timestamp belongs to that earlier sample, not to the failed attempt or MQTT publication time. The same failure-reporting pattern applies to BME280 and DS18B20.

Before any successful acquisition, data and timestamps remain at their initial zero values. Consumers must check `error_status` before treating a message as a successful new measurement. Sensor initialization failures may prevent the sampling task from starting; inspect active-device state and logs if no messages arrive.

## Troubleshooting

### Certificate parse error on ESP32

Example log:

```text
mbedtls_x509_crt_parse of CA cert returned -0x2180
ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED
```

Likely cause:

```text
The configured CA certificate is malformed, incomplete, or not a PEM certificate.
```

Fix:

```text
Copy the complete content of certs/ca.crt, including BEGIN/END CERTIFICATE lines.
```

### Certificate verification fails

Likely cause:

```text
The broker URI does not match the certificate subjectAltName.
```

Example:

```text
Broker URI: mqtts://192.168.178.68:8883
Certificate: subjectAltName = IP:192.168.178.68
```

These must match.

### Broker unavailable

If Mosquitto is stopped, the ESP32 should fail to connect but continue running.

Expected behavior:

```text
MQTT disconnect/error is logged
Enabled sensors continue acquisition
SNTP continues working
device does not reboot
MQTT reconnects after Mosquitto is started again
```

### No payloads received

Check:

```text
ESP32 is connected to Wi-Fi STA
broker URI is correct
CA certificate is correct
Mosquitto is running
mosquitto_sub uses the same CA certificate
subscriber topic is '#'
At least one connected sensor is enabled and active
DS18B20 ROM address or TL136 converter settings match the hardware
MQTT status is CONNECTED
```

If messages arrive with a non-`ESP_OK` `error_status`, MQTT transport is working; inspect the corresponding sensor's wiring, power, address, and status. A false `sample_time_valid` with `ESP_OK` indicates a successful reading without a valid UTC sample timestamp.
