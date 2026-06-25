# ESP32 AWS IoT Capstone

This project is an ESP32-S3 / ESP-IDF C++ capstone firmware project for an IoT sensor node.

The firmware combines Wi-Fi provisioning, HTTPS-based configuration, BME280 sensor sampling, SNTP time synchronization, MQTT publishing over TLS, and HTTPS OTA firmware updates.

The project is intentionally structured as a set of small ESP-IDF components with clear responsibilities and event-based interaction.

**Note:** The current firmware supports generic MQTT over TLS. Direct AWS IoT Core connectivity is not yet supported because MQTT client-certificate and private-key authentication are not yet implemented.

## 1. Overview

The ESP32 starts in a mode that provides a SoftAP and an HTTPS configuration page.  
The configuration page is used to provision Wi-Fi STA credentials and runtime settings for the sensor, SNTP, MQTT, and OTA functionality.

After Wi-Fi STA connectivity is available, the device can synchronize time using SNTP, publish BME280 sensor values via MQTT over TLS, and download OTA firmware updates from a local or remote HTTPS server.

The main runtime concept is event-based interaction between components:

```text
Wi-Fi events
- update runtime status
- trigger SNTP and MQTT connection handling

BME280 sample events
- update sensor status
- trigger MQTT publishing

Config update events
- notify components that runtime configuration changed

OTA request events
- trigger HTTPS OTA update handling
```

Shared runtime state is collected in the StateMachine component.  
Persistent configuration is stored in NVS by the ConfigStore component.

## 2. Hardware

The current test hardware is:

```text
ESP32-S3 N16R8 development board
- 16 MB flash
- 8 MB octal PSRAM

BME280 environmental sensor
- temperature
- pressure
- humidity
- I2C interface
```

Detailed hardware notes, wiring, pin layout, ESP-IDF target setup, and official hardware references are documented separately in [docs/HARDWARE.md](./docs/HARDWARE.md)


## 3. Architecture

The firmware is built as a C++ ESP-IDF application using multiple components.

The design goals are:

```text
clear component boundaries
event-based interaction
NVS-backed configuration
component-specific runtime status
TLS-only network communication for MQTT and OTA
simple local test setup for MQTT and OTA
```

The components do not directly control each other where this can be avoided.  
Instead, they exchange application events and read or update shared runtime status through the StateMachine.

Configuration is owned by ConfigStore and persisted in NVS.  
Runtime status is owned by StateMachine and separated into component-specific status domains.

## 4. Components

### app_cmn

Common project-wide definitions and helper types.

This component contains shared configuration structures, firmware metadata, common constants, and small utility functions used by other components.  
It is the lowest-level application component and should not depend on higher-level runtime components.

### app_resource_manager

Shared ESP-IDF resource initialization and lifecycle management.

This component initializes NVS flash, the TCP/IP network interface, and the default event loop used by the application.  
It provides a single owner for these shared platform resources and releases them during application shutdown.

### app_config_store

NVS-backed configuration storage.

This component owns the persistent application configuration and provides controlled access to the current configuration cache.  
Configuration updates are written to NVS and then propagated to interested components through application events.

### app_state_machine

Central runtime status and event coordination component.

The StateMachine stores component-specific runtime status, such as Wi-Fi, MQTT, SNTP, BME280, and OTA status.  
It provides thread-safe update and snapshot access and posts application events used to decouple components.

### app_wifi_manager

Wi-Fi SoftAP and STA management.

This component provides the provisioning access point and manages station-mode Wi-Fi connectivity.  
It updates Wi-Fi runtime status and posts events when network availability changes.

### app_https_server

HTTPS configuration and status web server.

This component serves the local configuration/status page over HTTPS.  
It accepts configuration updates, displays current runtime status, shows firmware metadata, and provides the user interface for triggering OTA updates.

### app_i2c_bus

Shared I2C bus abstraction.

This component initializes and owns the ESP-IDF I2C bus used by sensor components.  
It provides a controlled access point for I2C devices and keeps bus setup separate from sensor driver logic.

### app_i2c_bme280

BME280 sensor component.

This component detects the BME280 sensor, periodically samples temperature, pressure, and humidity, and updates the BME280 runtime status.  
Each successful sample posts a BME280 data-ready event that can be consumed by MQTT publishing.

Sensor data validity and timestamp validity are intentionally separate.  
BME280 values are valid when the sensor read succeeds. The UTC sample timestamp is valid only when device time is synchronized.

### app_sntp_client

SNTP time synchronization component.

This component synchronizes system time using DHCP-provided NTP servers, static NTP servers, or both.  
It tracks SNTP runtime state and exposes whether device time is valid for UTC sensor timestamps.

The component also handles stale-sync recovery after an initial successful synchronization.

### app_mqtt_client

MQTT publishing component.

This component manages the TLS MQTT client connection and publishes BME280 sensor values after BME280 data-ready events.  
MQTT uses server certificate verification with a configured CA certificate.

MQTT publishing does not require the BME280 UTC timestamp to be valid.  
If SNTP has not synchronized yet, payloads may contain valid sensor data with `sample_time_valid` set to `false`.

### app_ota_manager

HTTPS OTA update component.

This component performs firmware updates by downloading a new application image from a configured HTTPS URL.  
It verifies the TLS server certificate using the configured OTA server certificate and switches to the new OTA partition after a successful update.

## 5. SDK configuration

The project uses `sdkconfig.defaults` to document and preserve the important ESP-IDF configuration choices required for this firmware.

The most relevant settings are:

```text
ESP32-S3 target
16 MB flash size
custom partition table
OTA-capable partition layout
octal 80 MHz PSRAM
DHCP-provided NTP server support
SNTP server count set to 3
development log level
```

The project expects the ESP-IDF target to be configured as:

```bash
idf.py set-target esp32s3
```

Important project defaults are kept in:

```text
sdkconfig.defaults
```

This makes the project easier to reproduce on a fresh checkout.

A typical setup flow is:

```bash
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

The generated `sdkconfig` file is local build output and may contain machine-specific or temporary settings.  
The intended persistent project defaults should be maintained in `sdkconfig.defaults`.

The partition table is configured for 16 MB flash and OTA testing.  
The application uses a factory partition and OTA application partitions so that firmware updates can be downloaded, verified, and activated.

PSRAM is enabled because the firmware uses several memory-intensive components, including HTTPS, OTA, MQTT/TLS, and the configuration web interface.

SNTP DHCP NTP support is enabled so the firmware can use NTP servers provided by the local router in addition to statically configured NTP servers.

## 6. Build and flash

The project uses ESP-IDF v6.0.1 and targets ESP32-S3.

Typical build command from the project root:

```bash
idf.py build
```

Typical flash and monitor command:

```bash
idf.py flash monitor
```

Firmware metadata is embedded into the application and displayed on the HTTPS status/configuration page.

The metadata includes:

```text
firmware version
build date/time
IDF target
ESP-IDF version
```

## 7. Runtime configuration

Runtime configuration is performed through the ESP32 HTTPS configuration page.

After flashing and starting the ESP32:

1. Connect your computer or phone to the Wi-Fi network `esp-ssid` using the password `password$1`.
2. Open `https://192.168.4.1/config` in a web browser.
3. Sign in with the username `user` and password `password$1`.

The browser may display a certificate warning because the ESP32 uses a locally generated HTTPS certificate. Verify that the address is `192.168.4.1` before accepting the warning for this local connection.

These are the firmware's build-time defaults. They can be changed through `idf.py menuconfig` under **Component config > Wifi Manager** and **Component config > HTTPS Server**.

The configuration page is used for:

```text
Wi-Fi STA credentials
BME280 / I2C configuration
SNTP configuration
MQTT broker URI and CA certificate
OTA URL and OTA server certificate
```

The device also exposes the current component status and firmware metadata.

## 8. MQTT test setup

A local Mosquitto-based TLS broker test setup is provided in:

```text
mqtt-broker/
```

See: [mqtt-broker/README.md](./mqtt-broker/README.md)

That README explains how to:

```text
install Mosquitto tools
create local MQTT test certificates
start Mosquitto with TLS
start mosquitto_sub
configure the ESP32 MQTT broker URI and CA certificate
verify received BME280 JSON payloads
```

## 9. OTA test setup

A local OpenSSL-based HTTPS OTA test server setup is provided in:

```text
ota-server/
```

See: [ota-server/README.md](./ota-server/README.md)


That README explains how to:

```text
create a local HTTPS server certificate
build a versioned OTA firmware binary
start openssl s_server
configure the ESP32 OTA URL and server certificate
trigger and verify an OTA update
```

## 10. Current project status

The firmware feature implementation is complete for version 0.1.

Verified so far:

```text
build and flash
SoftAP visibility
HTTPS configuration/status page
NVS-backed configuration persistence
BME280 sampling
SNTP with DHCP-provided NTP server
SNTP with static NTP server
MQTT TLS publishing to local Mosquitto
MQTT reconnect after broker restart
MQTT safe failure with invalid CA certificate
HTTPS OTA update using local OpenSSL server
firmware metadata display
```

## 11. Known limitations

This is a private capstone project and not a production-ready IoT product.

Known limitations:

```text
local test certificates are for development only
no MQTT username/password support
no direct AWS IoT Core connectivity
no MQTT client-certificate or private-key authentication
OTA test setup uses a local development HTTPS server
security hardening is intentionally limited to the capstone scope
```
