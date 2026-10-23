#pragma once

#include <functional>
#include <memory>
#include "esp_err.h"
#include "app/stat/types.hpp"

namespace app::sm
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Thread-safe store for component status domains and application events.
     * Callbacks execute while their domain mutex is held and must not re-enter
     * StateMachine accessors for the same domain.
     */
    class StateMachine
    {
    public:
        StateMachine();

        ~StateMachine();

        StateMachine(const StateMachine &)            = delete;
        StateMachine &operator=(const StateMachine &) = delete;

        /** Creates synchronization resources. Idempotent after success. */
        esp_err_t init();

        /** Updates one status domain synchronously under its mutex. */
        void updateWifiStatus(const std::function<void(app::wm::WifiStatus &)> &);
        void updateOtaStatus(const std::function<void(app::ota::OtaStatus &)> &);
        void updateMqttStatus(const std::function<void(app::mqtt::MqttClientStatus &)> &);
        void updateSntpStatus(const std::function<void(app::sntp::SntpClientStatus &)> &);
        void updateDevicesStatus(const std::function<void(app::dm::DeviceManagerStatus &)> &);
        void updateBme280Status(const std::function<void(app::i2c::bme280::Bme280Status &)> &);
        void updateDs18b20Status(const std::function<void(app::onewire::ds18b20::Ds18b20Status &)> &);
        void updateTl136Status(const std::function<void(app::modbus::tl136::Tl136Status &)> &);

        /** Clears all status domains and releases synchronization resources. */
        void fini();

        /** Clears all status domains while keeping synchronization available. */
        esp_err_t reset();

        /** Returns whether synchronization resources are available. */
        bool isInitialized() const;

        /** Copies a cross-domain snapshot while holding all domain mutexes. */
        void getSnapshot(SystemStatus &) const;

        /** Copies the Wi-Fi status under its mutex. */
        void getWifiStatus(app::wm::WifiStatus &) const;
        /** Copies the OTA status under its mutex. */
        void getOtaStatus(app::ota::OtaStatus &) const;
        /** Copies the MQTT status under its mutex. */
        void getMqttStatus(app::mqtt::MqttClientStatus &) const;
        /** Copies the SNTP status under its mutex. */
        void getSntpStatus(app::sntp::SntpClientStatus &) const;
        /** Copies the device availability flags under their mutex. */
        void getDevicesStatus(app::dm::DeviceManagerStatus &) const;
        /** Copies the BME280 status under its mutex. */
        void getBme280Status(app::i2c::bme280::Bme280Status &) const;
        /** Copies the DS18B20 status under its mutex. */
        void getDs18b20Status(app::onewire::ds18b20::Ds18b20Status &) const;
        /** Copies the TL136 status under its mutex. */
        void getTl136Status(app::modbus::tl136::Tl136Status &) const;

        /** Posts a payload-free event and may block until the default loop accepts it. */
        esp_err_t postEvent(esp_event_base_t, int32_t event_id);

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
