# Local MQTT TLS Test Broker

This folder contains a minimal local Mosquitto setup for testing the ESP32 MQTT publish path with TLS.

The general ESP32 setup, including Wi-Fi provisioning, AP access, sensor setup, SNTP setup, build and flash instructions, is described in the [top-level project README](../README.md).

This README only describes the local MQTT broker test setup.

## 1. Test setup

The MQTT test verifies the following path:

```text
BME280 sample
- Event posted for BME280 data ready
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

## Expected result

With a correct broker URI and CA certificate, the ESP32 should connect to Mosquitto via TLS and publish BME280 payloads.

Expected firmware behavior:

```text
Wi-Fi STA connected
MQTT client starts
MQTT_EVENT_CONNECTED
BME280 samples continue
APP_EVENT_BME280_DATA_READY is posted
MQTT publishes BME280 JSON
```

Expected output after the ESP32 connects and publishes:

```text
devices/001-esp32s3/01-bme280 {"data":{"temp_degCx100":2843,"press_PaQ24d8":25653085,"hum_RhQ22d10":54210},"sample_time_valid":true,"sample_time_utc_us":1782152669572637}
```

The exact values depend on the current sensor readings and whether SNTP time synchronization is already valid.

Before SNTP synchronization, payloads may look like this:

```json
{
  "data": {
    "temp_degCx100": 2843,
    "press_PaQ24d8": 25653085,
    "hum_RhQ22d10": 54210
  },
  "sample_time_valid": false,
  "sample_time_utc_us": 0
}
```

After SNTP synchronization, later payloads should contain:

```json
{
  "sample_time_valid": true,
  "sample_time_utc_us": 1782152669572637
}
```

BME280 data validity is independent from timestamp validity.

The firmware may publish valid BME280 data even when `sample_time_valid` is `false`.

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
BME280 continues sampling
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
BME280 sampling is active
MQTT status is CONNECTED
```
