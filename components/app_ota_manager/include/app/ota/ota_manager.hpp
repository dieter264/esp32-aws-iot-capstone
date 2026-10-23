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

namespace app::ota
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Owns the event-driven OTA worker task and publishes OTA status updates.
     * Referenced ConfigStore and StateMachine instances must outlive this object.
     */
    class OtaManager
    {
    public:
        OtaManager(const app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~OtaManager();

        OtaManager(const OtaManager &)            = delete;
        OtaManager &operator=(const OtaManager &) = delete;

        /** Registers the OTA request handler and starts the task. Idempotent after success. */
        esp_err_t init();

        /**
         * Requests cooperative task shutdown and waits up to TASK_SHUTDOWN_TIMEOUT_MS.
         * On timeout, task resources remain intact so a second task cannot be started.
         */
        void fini();

        /** Restarts the manager using the latest configuration. */
        esp_err_t reset();

        /** Returns whether initialization completed successfully. */
        bool isInitialized() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
