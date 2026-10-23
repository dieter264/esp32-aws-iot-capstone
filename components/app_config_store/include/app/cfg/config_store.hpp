#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <memory>
#include "esp_err.h"
#include "app/cfg/types.hpp"

namespace app::cfg
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Thread-safe owner of the cached and NVS-backed application configuration.
     * Initialize once before dependent components; returned data is always copied.
     */
    class ConfigStore
    {
    public:
        ConfigStore();

        ~ConfigStore();

        ConfigStore(const ConfigStore &)            = delete;
        ConfigStore &operator=(const ConfigStore &) = delete;

        /** Loads defaults and persisted values. Idempotent after success. */
        esp_err_t init();

        /** Atomically persists a complete snapshot, then replaces the cache on success. */
        esp_err_t update(const AppConfig &);

        /** Clears the in-memory cache; does not erase persisted NVS values. */
        void fini();

        /** Finalizes and reloads configuration from NVS. */
        esp_err_t reset();

        /** Returns whether the configuration cache is ready for access. */
        bool isInitialized() const;

        /** Copies a consistent snapshot. May block briefly on the cache mutex. */
        void getSnapshot(AppConfig &) const;

        /** Copies the I2C bus configuration from the cache. */
        void getI2cBusConfig(app::i2c::bus::I2cBusConfig &) const;
        /** Copies the BME280 configuration from the cache. */
        void getBme280Config(app::i2c::bme280::Bme280Config &) const;
        /** Copies the Wi-Fi SoftAP configuration from the cache. */
        void getWifiApConfig(app::wm::WifiManagerApConfig &) const;
        /** Copies the Wi-Fi station configuration from the cache. */
        void getWifiStaConfig(app::wm::WifiManagerStaConfig &) const;
        /** Copies the HTTPS server configuration from the cache. */
        void getHttpsServerConfig(app::https::HttpsServerConfig &) const;
        /** Copies the OTA manager configuration from the cache. */
        void getOtaManagerConfig(app::ota::OtaManagerConfig &) const;
        /** Copies the MQTT client configuration from the cache. */
        void getMqttClientConfig(app::mqtt::MqttClientConfig &) const;
        /** Copies the SNTP client configuration from the cache. */
        void getSntpClientConfig(app::sntp::SntpClientConfig &) const;
        /** Copies the enabled-device configuration from the cache. */
        void getDeviceManagerConfig(app::dm::DeviceManagerConfig &) const;
        /** Copies the 1-Wire bus configuration from the cache. */
        void getOneWireBusConfig(app::onewire::bus::OneWireBusConfig &) const;
        /** Copies the DS18B20 configuration from the cache. */
        void getDs18b20Config(app::onewire::ds18b20::Ds18b20Config &) const;
        /** Copies the Modbus bus configuration from the cache. */
        void getModbusBusConfig(app::modbus::bus::ModbusBusConfig &) const;
        /** Copies the TL136 configuration from the cache. */
        void getTl136Config(app::modbus::tl136::Tl136Config &) const;

    private:
        bool isInitialized_ = false;
        struct State;
        std::unique_ptr<State> state_;
    };

    /** Copies a C string with truncation and always null-terminates the destination. */
    template <typename ElemT, size_t N>
    bool cstrCopy(ElemT (&dst)[N], const char *src)
    {
        static_assert(std::is_same_v<ElemT, char> || std::is_same_v<ElemT, uint8_t>,
                      "cstrCopy only supports char[] and uint8_t[]");

        static_assert(N > 0, "Destination array must not be empty");

        if (nullptr == src)
        {
            dst[0] = 0;
            return false;
        }

        const size_t srcLen  = strlen(src);
        const size_t copyLen = (srcLen < (N - 1)) ? srcLen : (N - 1);

        memcpy(dst, src, copyLen);
        dst[copyLen] = 0;

        return (srcLen < N);
    }

    /** Copies up to N bytes; the destination is not necessarily null-terminated. */
    template <typename ElemT, size_t N>
    size_t bufCopy(ElemT (&dst)[N], const char *src)
    {
        static_assert(std::is_same_v<ElemT, char> || std::is_same_v<ElemT, uint8_t>,
                      "bufCopy only supports char[] and uint8_t[]");

        static_assert(N > 0, "Destination array must not be empty");

        if (nullptr == src)
            return 0;

        const size_t srcLen  = strlen(src);
        const size_t copyLen = (srcLen < N) ? srcLen : N;

        memcpy(dst, src, copyLen);
        return copyLen;
    }
}
