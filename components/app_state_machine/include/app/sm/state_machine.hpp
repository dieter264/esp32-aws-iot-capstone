#pragma once

#include <functional>
#include <memory>
#include "esp_err.h"
#include "app/stat/types.hpp"

namespace app::sm
{
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
        void updateBme280Status(const std::function<void(app::i2c::bme280::Bme280Status &)> &);
        void updateMqttStatus(const std::function<void(app::mqtt::MqttClientStatus &)> &);
        void updateSntpStatus(const std::function<void(app::sntp::SntpClientStatus &)> &);

        /** Clears all status domains and releases synchronization resources. */
        void fini();

        esp_err_t reset();

        bool isInitialized() const;

        /** Copies a cross-domain snapshot while holding all domain mutexes. */
        void getSnapshot(SystemStatus &) const;

        void getWifiStatus(app::wm::WifiStatus &) const;
        void getOtaStatus(app::ota::OtaStatus &) const;
        void getBme280Status(app::i2c::bme280::Bme280Status &) const;
        void getMqttStatus(app::mqtt::MqttClientStatus &) const;
        void getSntpStatus(app::sntp::SntpClientStatus &) const;

        /** Posts a payload-free event and may block until the default loop accepts it. */
        esp_err_t postEvent(esp_event_base_t, int32_t event_id);

    private:
        bool isInitialized_ = false;
        struct State;
        std::unique_ptr<State> state_;
    };
}
