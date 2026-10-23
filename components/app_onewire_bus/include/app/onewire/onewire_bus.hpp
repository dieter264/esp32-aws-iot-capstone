#pragma once

#include <memory>
#include "esp_err.h"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::onewire::bus
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /** Owns one ESP-IDF 1-Wire bus and the devices discovered on it. */
    class OneWireBus
    {
    public:
        OneWireBus(const app::cfg::ConfigStore &);

        ~OneWireBus();

        OneWireBus(const OneWireBus &)            = delete;
        OneWireBus &operator=(const OneWireBus &) = delete;

        /** Creates the bus and discovers attached devices. */
        esp_err_t init();

        /** Releases discovered devices and the underlying bus. */
        void fini();

        /** Reinitializes the bus using the latest ConfigStore snapshot. */
        esp_err_t reset();

        /** Returns whether the bus was initialized successfully. */
        bool isInitialized() const;

        /** Returns whether the stored bus configuration differs from the active one. */
        bool hasConfigChanged() const;

        /**
         * Sends a command and polls until the device signals completion.
         * @param devAddr 64-bit 1-Wire ROM address.
         * @param txCmd Command bytes to transmit.
         * @param txCmdLen Number of command bytes.
         * @param timeoutMillis Maximum polling time in milliseconds.
         */
        esp_err_t writeCmdPolling(uint64_t devAddr,
                                  const uint8_t *txCmd,
                                  uint8_t txCmdLen,
                                  uint32_t timeoutMillis = 1000u);

        /** Sends a command and reads exactly @p rxBufLen response bytes. */
        esp_err_t readData(uint64_t devAddr,
                           const uint8_t *txCmd,
                           uint8_t txCmdLen,
                           uint8_t *rxBuf,
                           size_t rxBufLen);

        /** Sends a command followed by @p txBufLen data bytes. */
        esp_err_t writeData(uint64_t devAddr,
                            const uint8_t *txCmd,
                            uint8_t txCmdLen,
                            const uint8_t *txBuf,
                            uint8_t txBufLen);

        /** Returns whether the final byte equals the 1-Wire CRC-8 of prior bytes. */
        bool verifyCrc8(uint8_t *buf, size_t bufLen) const;

        /** Logs the ROM addresses of all devices discovered during initialization. */
        void logConnectedDevices() const;

        /** Returns whether @p devAddr was discovered during initialization. */
        bool isDeviceAvailable(uint64_t devAddr) const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
