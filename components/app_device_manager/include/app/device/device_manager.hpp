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

namespace app::dm
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Starts, stops, and monitors the devices enabled in DeviceManagerConfig.
     * Constructor dependencies are non-owning and must outlive this object.
     */
    class DeviceManager
    {
    public:
        DeviceManager(app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~DeviceManager();

        DeviceManager(const DeviceManager &)            = delete;
        DeviceManager &operator=(const DeviceManager &) = delete;

        /** Creates enabled devices and starts the monitoring task. */
        esp_err_t init();

        /** Stops monitoring and releases all managed devices and buses. */
        void fini();

        /** Recreates managed devices from the latest configuration. */
        esp_err_t reset();

        /** Returns whether initialization completed successfully. */
        bool isInitialized() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
