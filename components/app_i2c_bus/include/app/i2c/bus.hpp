#pragma once

#include <memory>
#include "esp_err.h"
#include "app/cfg/types.hpp"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::i2c::bus
{
    void apply_log_level();

    /** Owns one ESP-IDF I2C master bus configured by ConfigStore. */
    class I2cBus
    {
    public:
        I2cBus(const app::cfg::DeviceID, const app::cfg::ConfigStore &);

        ~I2cBus();

        I2cBus(const I2cBus &) = delete;
        I2cBus &operator=(const I2cBus &) = delete;

        /** Creates the master bus. The referenced ConfigStore must be initialized. */
        esp_err_t init();

        /** Deletes the master bus; all attached device users must already be stopped. */
        void fini();

        esp_err_t reset();

        bool isInitialized() const;

        /**
         * Probes for chipID, preferring the input devAddress and scanning if needed.
         * This synchronous bus operation updates devAddress on success.
         */
        esp_err_t getDevAddressForChipID(uint8_t chipID, uint8_t regAddress,
                                         uint16_t &devAddress);

    private:
        bool isInitialized_ = false;
        const app::cfg::DeviceID devID_;
        const app::cfg::ConfigStore &cfgStore_;
        struct State;
        std::unique_ptr<State> state_;
    };
}
