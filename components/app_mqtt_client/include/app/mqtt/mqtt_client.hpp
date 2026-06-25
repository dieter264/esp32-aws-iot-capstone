#pragma once

#include <memory>
#include "esp_err.h"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::sm
{
    class StateMachine;
}

namespace app::mqtt
{
    void apply_log_level();

    /**
     * Owns the TLS MQTT client and event-driven worker task.
     * Publishes valid BME280 samples on APP_EVENT_BME280_DATA_READY even when
     * their UTC timestamp is invalid. Dependencies are non-owning.
     */
    class MqttClient
    {
    public:
        MqttClient(const app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~MqttClient();

        MqttClient(const MqttClient &)            = delete;
        MqttClient &operator=(const MqttClient &) = delete;

        /** Registers event handlers and starts the worker task. Idempotent after success. */
        esp_err_t init();

        /**
         * Requests cooperative task shutdown and waits up to TASK_SHUTDOWN_TIMEOUT_MS.
         * On timeout, task resources remain intact so a second task cannot be started.
         */
        void fini();

        esp_err_t reset();

        bool isInitialized() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
