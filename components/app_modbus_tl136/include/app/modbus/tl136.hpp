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

namespace app::modbus::bus
{
    class ModbusBus;
}

namespace app::modbus::tl136
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Owns a TL136 water-level sensor and its periodic sampling task.
     * Constructor dependencies are non-owning and must outlive this object.
     */
    class Tl136
    {
    public:
        explicit Tl136(app::cfg::ConfigStore &,
                       app::sm::StateMachine &,
                       app::modbus::bus::ModbusBus &);

        ~Tl136();

        Tl136(const Tl136 &)            = delete;
        Tl136 &operator=(const Tl136 &) = delete;

        /** Configures the converter and starts periodic sampling. */
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
