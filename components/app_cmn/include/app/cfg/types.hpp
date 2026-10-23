#pragma once

#include "sdkconfig.h"
#include "stdint.h"
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>

namespace app::cfg
{
    /** Maximum time task-owning components wait for cooperative STOP_ACK. */
    constexpr uint32_t TASK_SHUTDOWN_TIMEOUT_MS{5000};
}

namespace app::i2c::bus
{
    /** Runtime I2C bus timing and port configuration. */
    struct I2cBusConfig
    {
        uint32_t scl_speed_hz = CONFIG_APP_I2C_BUS_SCL_SPEED_HZ;
        int xfer_timeout_ms   = CONFIG_APP_I2C_BUS_XFER_TIMEOUT_MS;
        int i2c_port_num      = 0;

        bool operator==(const I2cBusConfig &other) const
        {
            return (scl_speed_hz == other.scl_speed_hz) &&
                   (xfer_timeout_ms == other.xfer_timeout_ms) &&
                   (i2c_port_num == other.i2c_port_num);
        }

        bool operator!=(const I2cBusConfig &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::i2c::bme280
{
    /** BME280 oversampling register values. */
    enum class Oversampling : uint8_t
    {
        skipped = 0,
        x1      = 1,
        x2      = 2,
        x4      = 3,
        x8      = 4,
        x16     = 5
    };

    /** BME280 operating modes. */
    enum class Mode : uint8_t
    {
        sleep  = 0,
        forced = 1,
        normal = 3
    };

    /** BME280 normal-mode standby durations. */
    enum class Standby : uint8_t
    {
        ms0p5  = 0,
        ms62p5 = 1,
        ms125  = 2,
        ms250  = 3,
        ms500  = 4,
        ms1000 = 5,
        ms10   = 6,
        ms20   = 7
    };

    /** BME280 IIR filter coefficients. */
    enum class Filter : uint8_t
    {
        off = 0,
        c2  = 1,
        c4  = 2,
        c8  = 3,
        c16 = 4
    };

    /** BME280 address and measurement configuration. */
    struct Bme280Config
    {
        uint16_t device_address{};
        uint32_t probe_interval_ms{};

        Oversampling osrs_t = Oversampling::x2;
        Oversampling osrs_p = Oversampling::x16;
        Oversampling osrs_h = Oversampling::x1;

        Mode mode     = Mode::normal;
        Standby t_sb  = Standby::ms500;
        Filter filter = Filter::c16;

        bool operator==(const Bme280Config &other) const
        {
            return (device_address == other.device_address) &&
                   (probe_interval_ms == other.probe_interval_ms) &&
                   (osrs_t == other.osrs_t) &&
                   (osrs_p == other.osrs_p) &&
                   (osrs_h == other.osrs_h) &&
                   (mode == other.mode) &&
                   (t_sb == other.t_sb) &&
                   (filter == other.filter);
        }

        bool operator!=(const Bme280Config &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::wm
{
    /** Maximum Wi-Fi SSID size including its null terminator. */
    constexpr size_t SSID_BUF_SIZE    = 33;
    /** Maximum Wi-Fi password size including its null terminator. */
    constexpr size_t PASS_BUF_SIZE    = 64;
    /** IPv4 text buffer size including its null terminator. */
    constexpr size_t STA_IP_SIZE      = 16;
    /** Sentinel used when no station RSSI measurement is available. */
    constexpr int8_t RSSI_INVALID_DBM = -127;

    /** SoftAP credentials and radio channel; strings are null-terminated. */
    struct WifiManagerApConfig
    {
        char ssid[SSID_BUF_SIZE]     = {};
        char password[PASS_BUF_SIZE] = {};
        uint8_t channel              = 0;
    };

    /** Station credentials; strings are null-terminated. */
    struct WifiManagerStaConfig
    {
        char ssid[SSID_BUF_SIZE]     = {};
        char password[PASS_BUF_SIZE] = {};

        bool operator==(const WifiManagerStaConfig &other) const
        {
            return (0 == strcmp(ssid, other.ssid)) &&
                   (0 == strcmp(password, other.password));
        }

        bool operator!=(const WifiManagerStaConfig &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::https
{
    /** Maximum Basic Authentication user name size including the terminator. */
    constexpr size_t USER_BUF_SIZE = 32;
    /** Maximum Basic Authentication password size including the terminator. */
    constexpr size_t PASS_BUF_SIZE = 64;

    /** HTTPS Basic Authentication credentials; strings are null-terminated. */
    struct HttpsServerConfig
    {
        char username[USER_BUF_SIZE] = {};
        char password[PASS_BUF_SIZE] = {};
    };
}

namespace app::ota
{
    /** Maximum OTA URL size including its null terminator. */
    constexpr size_t URL_SIZE         = 193;
    /** Reserved certificate capacity used by configuration validation. */
    constexpr size_t SERVER_CERT_SIZE = 2048;

    /** OTA image URL and trusted server certificate in PEM form. */
    struct OtaManagerConfig
    {
        char url[URL_SIZE]      = {};
        std::string server_cert = {};
    };
}

namespace app::mqtt
{
    /** Maximum broker URI size including its null terminator. */
    constexpr size_t URI_SIZE         = 193;
    /** Reserved certificate capacity used by configuration validation. */
    constexpr size_t BROKER_CERT_SIZE = 2048;

    /** MQTT broker URI and trusted broker certificate in PEM form. */
    struct MqttClientConfig
    {
        char uri[URI_SIZE]      = {};
        std::string broker_cert = {};

        bool operator==(const MqttClientConfig &other) const
        {
            return (0 == strcmp(uri, other.uri)) &&
                   (broker_cert == other.broker_cert);
        }

        bool operator!=(const MqttClientConfig &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::sntp
{
    /** Number of supported statically configured SNTP servers. */
    constexpr size_t MAX_STATIC_SERVERS    = 2;
    /** Maximum SNTP host name size including its null terminator. */
    constexpr size_t SNTP_SERVER_NAME_SIZE = 64;

    /**
     * SNTP enablement and server sources.
     *
     * DHCP-provided and static servers may be enabled together. Static server
     * strings are null-terminated and must be configured in array order.
     */
    struct SntpClientConfig
    {
        bool sntp_enabled     = false;
        bool server_from_dhcp = false;
        char static_sntp_server[MAX_STATIC_SERVERS]
                               [SNTP_SERVER_NAME_SIZE] = {};

        bool operator==(const SntpClientConfig &other) const
        {
            if ((sntp_enabled != other.sntp_enabled) ||
                (server_from_dhcp != other.server_from_dhcp))
            {
                return false;
            }

            for (size_t i = 0; i < MAX_STATIC_SERVERS; i++)
            {
                if (0 != strcmp(static_sntp_server[i], other.static_sntp_server[i]))
                {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const SntpClientConfig &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::dm
{
    /** Logical device identifiers used to select component configuration. */
    enum DeviceID : uint32_t
    {
        I2C_BME280      = 1u << 0,
        ONEWIRE_DS18B20 = 1u << 1,
        MODBUS_TL136    = 1u << 2
    };

    /** Mask selecting all I2C devices. */
    constexpr auto I2C_DEVICE_MASK =
        DeviceID::I2C_BME280;

    /** Mask selecting all 1-Wire devices. */
    constexpr auto ONEWIRE_DEVICE_MASK =
        DeviceID::ONEWIRE_DS18B20;

    /** Mask selecting all Modbus devices. */
    constexpr auto MODBUS_DEVICE_MASK =
        DeviceID::MODBUS_TL136;

    /** Mask covering every possible device flag. */
    constexpr auto ALL_DEVICE_MASK =
        UINT32_MAX;

    /** Set of device flags used for configuration and runtime availability. */
    struct FlagField
    {
        uint32_t deviceFlags{};

        /** Returns whether the specified device flag is set. */
        bool hasFlag(DeviceID deviceID) const
        {
            return 0 != (deviceFlags & deviceID);
        }

        /** Adds the specified device flag. */
        void setFlag(DeviceID deviceID)
        {
            deviceFlags |= deviceID;
        }

        /** Removes the specified device flag. */
        void rmFlag(DeviceID deviceID)
        {
            deviceFlags &= ~deviceID;
        }

        bool operator==(const FlagField &) const = default;
    };

    using DeviceManagerConfig = FlagField;
}

namespace app::onewire::bus
{
    /** 1-Wire GPIO and receive-buffer configuration. */
    struct OneWireBusConfig
    {
        int gpio_num{};
        uint32_t max_rx_bytes{};

        bool operator==(const OneWireBusConfig &other) const
        {
            return (other.gpio_num == gpio_num) &&
                   (other.max_rx_bytes == max_rx_bytes);
        }

        bool operator!=(const OneWireBusConfig &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::onewire::ds18b20
{
    /** DS18B20 ROM address and sampling interval. */
    struct Ds18b20Config
    {
        uint64_t address{};
        uint32_t probe_interval_ms{};

        bool operator==(const Ds18b20Config &other) const
        {
            return (address == other.address) &&
                   (probe_interval_ms == other.probe_interval_ms);
        }

        bool operator!=(const Ds18b20Config &other) const
        {
            return !(*this == other);
        }
    };
}

namespace app::modbus::bus
{
    /** Supported RS-485 transceiver direction-control modes. */
    enum UART_RS485_Transceiver : uint8_t
    {
        MAX485,
        MAX13487E
    };

    /** UART parity settings encoded for the ESP-IDF UART driver. */
    enum UART_Parity : uint8_t
    {
        NONE = 0x0,
        EVEN = 0x2,
        ODD  = 0x3
    };

    /** UART stop-bit settings encoded for the ESP-IDF UART driver. */
    enum UART_Stop_Bits : uint8_t
    {
        ONE = 0x1,
        TWO = 0x3
    };
    /** Modbus RTU UART, RS-485, and response-timeout configuration. */
    struct ModbusBusConfig
    {
        uint8_t uart_port{};
        uint8_t uart_tx_gpio{};
        uint8_t uart_rx_gpio{};
        UART_RS485_Transceiver uart_rs485_transceiver{};
        std::optional<uint8_t> uart_rts_gpio{};
        uint32_t uart_baud_rate{};
        uint32_t response_tout_ms{};
        UART_Parity uart_parity{};
        UART_Stop_Bits uart_stop_bits{};

        bool operator==(const ModbusBusConfig &) const = default;
    };
}

namespace app::modbus::tl136
{
    /** TL136 converter channel, calibration, range, and sampling settings. */
    struct Tl136Config
    {
        uint8_t ai_converter_address{};
        uint8_t ai_converter_channel{};
        uint16_t ai_converter_calibration{};
        uint32_t probe_interval_ms{};
        uint16_t full_scale_water_level_mm{};

        bool operator==(const Tl136Config &) const = default;
    };
}

namespace app::cfg
{
    /** Complete configuration snapshot owned and persisted by ConfigStore. */
    struct AppConfig
    {
        app::i2c::bus::I2cBusConfig i2cBus;
        app::i2c::bme280::Bme280Config bme280;
        app::wm::WifiManagerApConfig wifiMgrAP;
        app::wm::WifiManagerStaConfig wifiMgrSta;
        app::https::HttpsServerConfig httpsSrv;
        app::ota::OtaManagerConfig otaMgr;
        app::mqtt::MqttClientConfig mqttClt;
        app::sntp::SntpClientConfig sntpClt;
        app::dm::DeviceManagerConfig deviceMgr;
        app::onewire::bus::OneWireBusConfig oneWireBus;
        app::onewire::ds18b20::Ds18b20Config ds18b20;
        app::modbus::bus::ModbusBusConfig modbusBus;
        app::modbus::tl136::Tl136Config tl136;
    };
}
