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

namespace app::onewire::bus
{
    class OneWireBus;
}

namespace app::onewire::ds18b20
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Owns a DS18B20 device and its periodic sampling task.
     * Constructor dependencies are non-owning and must outlive this object.
     */
    class Ds18b20
    {
    public:
        Ds18b20(app::cfg::ConfigStore &,
                app::sm::StateMachine &,
                app::onewire::bus::OneWireBus &);

        ~Ds18b20();

        Ds18b20(const Ds18b20 &)            = delete;
        Ds18b20 &operator=(const Ds18b20 &) = delete;

        /** Attaches the configured device and starts periodic sampling. */
        esp_err_t init();

        /** Stops sampling and releases the device resources. */
        void fini();

        /** Reinitializes the sensor using the latest configuration. */
        esp_err_t reset();

        /** Returns whether initialization completed successfully. */
        bool isInitialized() const;

        /** Returns whether the stored sensor configuration differs from the active one. */
        bool hasConfigChanged() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
