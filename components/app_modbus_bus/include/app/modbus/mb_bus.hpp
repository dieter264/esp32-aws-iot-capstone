#pragma once

#include <cstdint>
#include <memory>

#include "esp_err.h"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::modbus::bus
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /** Owns a Modbus RTU master connected through an ESP-IDF UART port. */
    class ModbusBus
    {
    public:
        explicit ModbusBus(const app::cfg::ConfigStore &);

        ~ModbusBus();

        ModbusBus(const ModbusBus &)            = delete;
        ModbusBus &operator=(const ModbusBus &) = delete;

        /** Configures the UART and starts the Modbus master. */
        esp_err_t init();

        /** Stops the master and releases its UART resources. */
        void fini();

        /** Reinitializes the bus using the latest ConfigStore snapshot. */
        esp_err_t reset();

        /** Returns whether the Modbus master is ready for transactions. */
        bool isInitialized() const;

        /** Returns whether the stored bus configuration differs from the active one. */
        bool hasConfigChanged() const;

        /** Reads one holding register from a Modbus device into @p data. */
        esp_err_t readHoldingRegister(uint8_t deviceAddr,
                                      uint16_t registerAddr,
                                      uint16_t &data);

        /** Writes one holding register on a Modbus device. */
        esp_err_t writeHoldingRegister(uint8_t deviceAddr,
                                       uint16_t registerAddr,
                                       uint16_t data);

        /** Probes @p deviceAddr and returns ESP_OK if a device responds. */
        esp_err_t scanDeviceAddress(uint8_t deviceAddr);

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
