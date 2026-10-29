# ESP32 AWS IoT Capstone

This project is an ESP32-S3 / ESP-IDF C++ capstone firmware project for an IoT sensor node.

The firmware combines Wi-Fi provisioning, HTTPS-based configuration, configurable sensor management, SNTP time synchronization, MQTT publishing over TLS, and HTTPS OTA firmware updates. It supports BME280, DS18B20, and TL136 water-level sensors over I2C, 1-Wire, and Modbus RTU/RS-485 buses.

The project is intentionally structured as a set of small ESP-IDF components with clear responsibilities and event-based interaction.

**Note:** The current firmware supports generic MQTT over TLS. Direct AWS IoT Core connectivity is not yet supported because MQTT client-certificate and private-key authentication are not yet implemented.

## 1. Overview

The ESP32 starts in a mode that provides a SoftAP and an HTTPS configuration page.  
The configuration page is used to provision Wi-Fi STA credentials and runtime settings for the sensors, SNTP, MQTT, and OTA functionality.

After Wi-Fi STA connectivity is available, the device can synchronize time using SNTP, publish enabled sensor values via MQTT over TLS, and download OTA firmware updates from a local or remote HTTPS server.

The main runtime concept is event-based interaction between components:

```text
Wi-Fi events
- update runtime status
- trigger SNTP and MQTT connection handling

Sensor sample events
- update the BME280, DS18B20, or TL136 status domain
- trigger MQTT publishing for that sensor

Config update events
- notify components that runtime configuration changed
- reconfigure enabled devices and their buses when required

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

DS18B20 digital temperature sensor
- temperature
- 1-Wire interface

TL136 water-level sensor with R4CVA02 analog converter
- 4-20 mA water-level measurement
- Modbus RTU over RS-485
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

The StateMachine stores component-specific runtime status, such as Wi-Fi, MQTT, SNTP, device availability, sensor data, and OTA status.
It provides thread-safe update and snapshot access and posts application events used to decouple components.

### app_device_manager

Runtime sensor and bus lifecycle management.

This component creates the buses and sensors selected in the persistent configuration. It monitors configuration changes, restarts affected devices when required, and reports which configured devices are active.

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

### app_onewire_bus

Shared 1-Wire bus abstraction.

This component owns the ESP-IDF 1-Wire bus, discovers attached devices, and provides command, read, write, CRC, and device-availability operations for 1-Wire sensor components.

### app_onewire_ds18b20

DS18B20 temperature sensor component.

This component addresses a configured DS18B20 by its 64-bit ROM address, periodically measures temperature, updates runtime status, and posts a data-ready event after each measurement attempt.

### app_modbus_bus

Shared Modbus RTU master abstraction.

This component owns the UART and RS-485 Modbus master. It supports MAX485 transceivers with RTS direction control and MAX13487E transceivers with automatic direction control, and provides holding-register access for Modbus devices.

### app_modbus_tl136

TL136 water-level sensor component.

This component reads a TL136 4-20 mA sensor through an R4CVA02 Modbus analog-input converter. It applies the configured converter calibration, calculates water level from the configured full-scale range, updates runtime status, and posts data-ready events.

### app_sntp_client

SNTP time synchronization component.

This component synchronizes system time using DHCP-provided NTP servers, static NTP servers, or both.  
It tracks SNTP runtime state and exposes whether device time is valid for UTC sensor timestamps.

The component also handles stale-sync recovery after an initial successful synchronization.

### app_mqtt_client

MQTT publishing component.

This component manages the TLS MQTT client connection and publishes BME280, DS18B20, and TL136 sensor values after their data-ready events.
MQTT uses server certificate verification with a configured CA certificate.

MQTT publishing does not require a sensor's UTC timestamp to be valid.
If SNTP has not synchronized yet, payloads may contain valid sensor data with `sample_time_valid` set to `false`.

The current QoS 0 topics are:

```text
devices/001-esp32s3/01-bme280
devices/001-esp32s3/02-ds18b20
devices/001-esp32s3/03-tl136
```

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
application-wide log-level control
```

Before configuring the target or running the setup flow below, activate the ESP-IDF v6.1 environment and [create the device's HTTPS certificate and key](#61-create-the-devices-https-certificate). CMake requires those files even when setting the target for the first time.

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

Project-specific `menuconfig` options provide component log-level overrides, initial sensor enablement, sensor defaults, and I2C, 1-Wire, and Modbus bus settings.

## 6. Build and flash

The project uses ESP-IDF v6.1 and targets ESP32-S3.

### 6.1 Create the device's HTTPS certificate

The ESP32's configuration server embeds a local certificate and private key at build time. On a fresh checkout, generate them once from the project root:

```bash
mkdir -p components/app_https_server/certs
openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \
  -keyout components/app_https_server/certs/server.key \
  -out components/app_https_server/certs/server.crt \
  -subj "/CN=192.168.4.1" \
  -addext "subjectAltName = IP:192.168.4.1"
chmod 600 components/app_https_server/certs/server.key
```

Reuse existing files for later builds; running this command again replaces them. These development credentials are ignored by Git and must not be committed. This pair belongs to the ESP32's HTTPS configuration server; the MQTT broker and OTA test server use their own certificates, described in their setup guides.

The certificate is self-signed, so the browser will still require you to accept it for the local configuration page.

### 6.2 Build and flash

The standard workflow below assumes Secure Boot and Flash Encryption are disabled. To enable Secure Boot v2, Release-mode Flash Encryption, and NVS Encryption, follow [ESP32-S3 Hardware Security](./docs/ESP32-S3_HARDWARE_SECURITY.md) before flashing. The same partition table supports both SDK configurations.

Typical build command from the project root:

```bash
idf.py build
```

In addition to the normal build output, the completed build copies the application image to:

```text
build/firmware-v0.4.bin
```

Filenames use `v` followed by `FIRMWARE_VERSION` from [firmware.hpp](./components/app_cmn/include/app/cfg/firmware.hpp).

If `ota-server/bin/` exists, the same versioned image is copied there for OTA testing.

Typical flash and monitor command:

```bash
idf.py flash monitor
```

The partition table uses 4 MiB factory and OTA application slots. Devices running v0.1 or v0.3 need a full USB flash to adopt the v0.4 layout: the partition table and NVS offsets changed, even for boards already using 4 MiB application slots. An application-only OTA update cannot perform this migration. Record existing configuration before flashing and reprovision it afterwards; see the [OTA migration notes](./ota-server/README.md#migrating-to-v04). For hardware security provisioning, follow the security guide from a fresh, flash-erased board.

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

These are the firmware's build-time defaults. They can be changed through `idf.py menuconfig` under **Application Configuration > Wifi Manager** and **Application Configuration > HTTPS Server**.

The configuration page is used for:

```text
Wi-Fi STA credentials
enabling or disabling individual sensors
BME280 address, sampling, and oversampling configuration
DS18B20 ROM address and sampling interval
TL136 converter address/channel, calibration, sampling interval, and full-scale range
SNTP configuration
MQTT broker URI and CA certificate
OTA URL and OTA server certificate
```

I2C GPIOs, the 1-Wire GPIO, and Modbus UART/RS-485 settings are build-time options under **Application Configuration** in `idf.py menuconfig`. The device also exposes current component status, active-device state, sensor readings, and firmware metadata.

With fresh NVS configuration and the default build settings, all sensors are disabled. Enable the connected sensors on the configuration page and, for the DS18B20, enter its 64-bit ROM address. Existing NVS settings are loaded on subsequent boots.

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
verify received BME280, DS18B20, and TL136 JSON payloads
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

The current firmware includes optional Secure Boot v2, Release-mode Flash Encryption, and Flash Encryption-based NVS key protection. SDK configuration selects the unsecured or secured workflow described in [ESP32-S3 Hardware Security](./docs/ESP32-S3_HARDWARE_SECURITY.md).

Verified so far:

```text
build and flash
SoftAP visibility
HTTPS configuration/status page
NVS-backed configuration persistence
BME280 sampling
DS18B20 sampling over 1-Wire
TL136 water-level sampling through an R4CVA02 Modbus converter
runtime sensor enable/disable and device monitoring
SNTP with DHCP-provided NTP server
SNTP with static NTP server
MQTT TLS publishing of all supported sensor types to local Mosquitto
MQTT reconnect after broker restart
MQTT safe failure with invalid CA certificate
HTTPS OTA update using local OpenSSL server
firmware metadata display
versioned firmware binary generation
Secure Boot v2 and Release-mode Flash Encryption provisioning
encrypted NVS using a flash-protected key partition
signed OTA image and host-encrypted factory image generation
VS Code Full Clean -> Build Project -> Execute Custom Task -> Monitor Device
```

## 11. Known limitations

This is a capstone project and not a production-ready IoT product.

Known limitations:

```text
local test certificates are for development only
explicit build-time default credentials are provisional and require further security hardening
runtime changes of access credentials, including the SoftAP password, are not yet supported
no MQTT username/password support
no direct AWS IoT Core connectivity
no MQTT client-certificate or private-key authentication
OTA test setup uses a local development HTTPS server
```

Hardware security is opt-in; `sdkconfig.defaults` does not enable it. Once activated, security eFuses cannot be reset by changing SDK settings. JTAG debugging is disabled on the secured board, and later serial updates require the retained host key and a pre-encrypted image.

The custom serial task updates the factory application; it does not change an OTA-selected boot partition. Secure Download Mode does not authenticate the operator, so physical access can still permit destructive flash writes. Secure Boot does not by itself prevent rollback to older signed firmware, and neither Secure Boot nor Flash Encryption identifies a substituted board to the browser; see [HTTPS Server Identity](./docs/HTTPS_SERVER_IDENTITY.md).
