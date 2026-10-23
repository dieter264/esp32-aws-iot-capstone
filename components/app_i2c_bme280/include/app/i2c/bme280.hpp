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

namespace app::i2c::bus
{
    class I2cBus;
}

namespace app::i2c::bme280
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /**
     * Owns the BME280 device handle and periodic sampling task.
     * Constructor dependencies are non-owning and must outlive this object.
     */
    class Bme280
    {
    public:
        Bme280(app::cfg::ConfigStore &,
               app::sm::StateMachine &,
               app::i2c::bus::I2cBus &);

        ~Bme280();

        Bme280(const Bme280 &)            = delete;
        Bme280 &operator=(const Bme280 &) = delete;

        /** Initializes the device and starts periodic sampling. Idempotent after success. */
        esp_err_t init();

        /**
         * Requests cooperative task shutdown and waits up to TASK_SHUTDOWN_TIMEOUT_MS.
         * On timeout, task resources remain intact so a second task cannot be started.
         */
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
