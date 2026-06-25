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

        esp_err_t reset();

        bool isInitialized() const;

        const app::cfg::ConfigStore & getConfigStore() const;

        app::sm::StateMachine & getStateMachine();

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
