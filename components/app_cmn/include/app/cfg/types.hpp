#pragma once

#include "sdkconfig.h"
#include "stdint.h"
#include <cstddef>
#include <cstring>
#include <string>

namespace app::cfg
{
    /** Maximum time task-owning components wait for cooperative STOP_ACK. */
    constexpr uint32_t TASK_SHUTDOWN_TIMEOUT_MS{5000};

    /** Logical device identifiers used to select component configuration. */
    enum class DeviceID : uint8_t
    {
        I2C_BUS    = 0,
        I2C_BME280 = 1
    };
}

namespace app::i2c::bus
{
    /** Runtime I2C bus timing and port configuration. */
    struct I2cBusConfig
    {
        uint32_t scl_speed_hz = CONFIG_APP_I2C_BUS_SCL_SPEED_HZ;
        int xfer_timeout_ms   = CONFIG_APP_I2C_BUS_XFER_TIMEOUT_MS;
        int i2c_port_num      = 0;
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
        uint16_t device_address = 0;

        Oversampling osrs_t = Oversampling::x2;
        Oversampling osrs_p = Oversampling::x16;
        Oversampling osrs_h = Oversampling::x1;

        Mode mode     = Mode::normal;
        Standby t_sb  = Standby::ms500;
        Filter filter = Filter::c16;
    };
}

namespace app::wm
{
    constexpr size_t SSID_BUF_SIZE    = 33;
    constexpr size_t PASS_BUF_SIZE    = 64;
    constexpr size_t STA_IP_SIZE      = 16;
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
    constexpr size_t USER_BUF_SIZE = 32;
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
    constexpr size_t URL_SIZE         = 193;
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
    constexpr size_t URI_SIZE         = 193;
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
    constexpr size_t MAX_STATIC_SERVERS    = 2;
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
    };
}
