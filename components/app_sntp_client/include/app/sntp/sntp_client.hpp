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

namespace app::sntp
{
    void apply_log_level();

    /**
     * Owns DHCP/static-server SNTP setup and its event-driven worker task.
     * Referenced ConfigStore and StateMachine instances must outlive this object.
     */
    class SntpClient
    {
    public:
        SntpClient(const app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~SntpClient();

        SntpClient(const SntpClient &)            = delete;
        SntpClient &operator=(const SntpClient &) = delete;

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
