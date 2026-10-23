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

namespace app::wm
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Owns SoftAP/STA Wi-Fi and its event-driven worker task.
     * Referenced ConfigStore and StateMachine instances must outlive this object.
     */
    class WifiManager
    {
    public:
        WifiManager(const app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~WifiManager();

        WifiManager(const WifiManager &) = delete;
        WifiManager &operator=(const WifiManager &) = delete;

        /** Starts Wi-Fi, event handlers, and the worker task. Idempotent after success. */
        esp_err_t init();

        /**
         * Requests cooperative task shutdown and waits up to TASK_SHUTDOWN_TIMEOUT_MS.
         * On timeout, task resources remain intact so a second task cannot be started.
         */
        void fini();

        /** Restarts Wi-Fi using the latest configuration. */
        esp_err_t reset();

        /** Returns whether initialization completed successfully. */
        bool isInitialized() const;

        /** Returns the non-owning ConfigStore dependency. */
        const app::cfg::ConfigStore & getConfigStore() const;

        /** Returns the non-owning StateMachine dependency. */
        app::sm::StateMachine & getStateMachine();

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
