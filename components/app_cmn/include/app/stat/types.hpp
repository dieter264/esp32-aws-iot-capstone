#pragma once

#include <cstdint>
#include "esp_err.h"
#include "esp_event_base.h"
#include "app/cfg/types.hpp"

namespace app::wm
{
    /** Snapshot of SoftAP and station connectivity. */
    struct WifiStatus
    {
        bool ap_started{};
        bool sta_connected{};
        char ssid[SSID_BUF_SIZE]{};
        bool sta_has_ip{};
        char sta_ip[STA_IP_SIZE]{};
        int8_t rssi{RSSI_INVALID_DBM};
        uint32_t last_disconnect_reason{};
    };
}

namespace app::ota
{
    /** OTA request lifecycle reported by OtaManager. */
    enum class OtaState : uint8_t
    {
        /** No OTA request is active. */
        IDLE,
        /** An OTA request has been accepted by the event handler. */
        REQUESTED,
        /** Image download and validation are in progress. */
        RUNNING,
        /** The most recent OTA attempt failed. */
        FAILED,
        /** OTA succeeded and an application restart has been requested. */
        RESTART_PENDING
    };

    /** Latest OTA lifecycle state and operation result. */
    struct OtaStatus
    {
        OtaState state{OtaState::IDLE};
        int error_status{ESP_OK};
    };
}

namespace app::mqtt
{
    /** MQTT connection and most-recent publish lifecycle. */
    enum class MqttState : uint8_t
    {
        /** No lifecycle result has been reported yet. */
        UNKNOWN,
        /** Broker URI or trust certificate is absent. */
        NOT_CONFIGURED,
        /** The ESP-MQTT client is initialized but not running. */
        STOPPED,
        /** Connection establishment is in progress. */
        CONNECTING,
        /** The broker connection is established. */
        CONNECTED,
        /** A publish request was accepted or acknowledged. */
        PUBLISHED,
        /** The broker connection was lost. */
        DISCONNECTED,
        /** The most recent MQTT lifecycle operation failed. */
        ERROR
    };

    /**
     * Latest MQTT client state.
     *
     * A connected client publishes the latest valid BME280 and DS18B20 samples
     * after their data-ready events, regardless of sample_time_valid.
     */
    struct MqttClientStatus
    {
        MqttState state{MqttState::UNKNOWN};
        /** Monotonic microseconds at the most recent accepted publish request. */
        int64_t last_publish_us{};
    };
}

namespace app::sntp
{
    /** SNTP client lifecycle and synchronization progress. */
    enum class SntpState : uint8_t
    {
        /** No lifecycle result has been reported yet. */
        UNKNOWN,
        /** SNTP is disabled or has no usable server configuration. */
        NOT_CONFIGURED,
        /** esp_netif_sntp_init() succeeded; start is pending, usually for Wi-Fi IP. */
        INITIALIZED,
        /** esp_netif_sntp_start() succeeded; waiting for a sync event. */
        STARTED,
        /** At least one NETIF_SNTP_TIME_SYNC event has been received. */
        SYNCED,
        /** The initialized SNTP client has been stopped. */
        STOPPED,
        /** The most recent SNTP lifecycle operation failed. */
        ERROR
    };

    /**
     * Latest SNTP state and time-validity information.
     *
     * is_time_valid becomes true after the first sync event. Stale-sync
     * recovery is evaluated only after that first successful synchronization.
     */
    struct SntpClientStatus
    {
        SntpState state{SntpState::UNKNOWN};
        /** True only after at least one synchronization event. */
        bool is_time_valid{false};
        /** UTC timestamp supplied by the most recent synchronization event. */
        uint64_t last_sync_utc_us{0};
        esp_err_t error_status{ESP_OK};
    };
}

namespace app::dm
{
    using DeviceManagerStatus = FlagField;
}

namespace app::sensor_stat
{
    /** Latest sensor sample, its timestamp validity, and acquisition result. */
    template <typename Data>
    struct SensorStatus
    {
        Data data{};
        uint64_t sample_time_utc_us{};
        bool sample_time_valid{};
        esp_err_t error_status{ESP_OK};
    };
}

namespace app::i2c::bme280
{
    /** Compensated BME280 values in the sensor driver's fixed-point units. */
    struct Bme280Data
    {
        int32_t temp_degCx100{};
        uint32_t press_PaQ24d8{};
        uint32_t hum_RhQ22d10{};
    };

    using Bme280Status = app::sensor_stat::SensorStatus<Bme280Data>;
}

namespace app::onewire::ds18b20
{
    /** DS18B20 temperature in hundredths of a degree Celsius. */
    struct Ds18b20Data
    {
        int32_t temp_degCx100{};
    };

    using Ds18b20Status = app::sensor_stat::SensorStatus<Ds18b20Data>;
}

namespace app::modbus::tl136
{
    /** TL136 converter current and derived water-level measurement. */
    struct Tl136Data
    {
        uint16_t current_mAx100{};
        uint16_t water_level_mm{};
    };

    using Tl136Status = app::sensor_stat::SensorStatus<Tl136Data>;
}

namespace app::sm
{
    /** Thread-safe StateMachine snapshot across all component status domains. */
    struct SystemStatus
    {
        app::wm::WifiStatus wifi{};
        app::ota::OtaStatus ota{};
        app::dm::DeviceManagerStatus devices{};
        app::mqtt::MqttClientStatus mqtt{};
        app::sntp::SntpClientStatus sntp{};
        app::i2c::bme280::Bme280Status bme280{};
        app::onewire::ds18b20::Ds18b20Status ds18b20{};
        app::modbus::tl136::Tl136Status tl136{};
    };

    ESP_EVENT_DECLARE_BASE(APP_EVENT);

    /** Application event IDs posted on APP_EVENT; events carry no payload. */
    enum : int32_t
    {
        APP_EVENT_CONFIG_UPDATE,
        APP_EVENT_WIFI_UPDATE,
        APP_EVENT_OTA_REQUEST,
        APP_EVENT_RESTART_REQUEST,
        APP_EVENT_BME280_DATA_READY,
        APP_EVENT_DS18B20_DATA_READY,
        APP_EVENT_TL136_DATA_READY
    };
}
