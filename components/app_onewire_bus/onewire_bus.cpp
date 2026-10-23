#include <string>
#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "onewire_bus.h"
#include "onewire_cmd.h"
#include "onewire_crc.h"
#include "onewire_device.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/mutex.hpp"
#include "app/onewire/onewire_bus.hpp"

namespace app::onewire::bus
{
    constexpr auto TAG                   = "app_1wire_bus";
    constexpr auto ONEWIRE_BUS_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_ONEWIRE_BUS_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < ONEWIRE_BUS_LOG_LEVEL)
        {
            esp_log_level_set(TAG, ONEWIRE_BUS_LOG_LEVEL);
        }
    }

    struct OneWireBus::State
    {
        explicit State(const app::cfg::ConfigStore &cfgStore)
            : cfgStore_(cfgStore) {}

        bool isInitialized_ = false;
        OneWireBusConfig initialCfg_{};

        const app::cfg::ConfigStore &cfgStore_;

        onewire_bus_handle_t busHandle{};
        mutable app::cmn::Mutex lock_{};

        esp_err_t writeCmd(uint64_t devAddr,
                           const uint8_t *txCmd,
                           uint8_t txCmdLen);
    };

    OneWireBus::OneWireBus(const app::cfg::ConfigStore &cfgStore)
        : state_(std::make_unique<State>(cfgStore)) {}

    OneWireBus::~OneWireBus()
    {
        fini();
    }

    esp_err_t OneWireBus::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        state_->cfgStore_.getOneWireBusConfig(state_->initialCfg_);

        onewire_bus_config_t bus_config = {
            .bus_gpio_num = state_->initialCfg_.gpio_num,
            .flags        = {.en_pull_up = false}};

        onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = state_->initialCfg_.max_rx_bytes};

        esp_err_t ret = ESP_FAIL;

        LOGD_CALL(TAG, ret, onewire_new_bus_rmt,
                  (&bus_config, &rmt_config, &(state_->busHandle)),
                  "New 1-Wire bus created: GPIO=%d, max. RX bytes=%d",
                  bus_config.bus_gpio_num, rmt_config.max_rx_bytes);

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "1-Wire bus initialized");
        return ret;
    }

    void OneWireBus::fini()
    {
        if (nullptr != state_->busHandle)
        {
            onewire_bus_del(state_->busHandle);
            state_->busHandle = {};
        }

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "1-Wire bus finalized");
    }

    esp_err_t OneWireBus::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset 1-Wire bus");

        ESP_LOGD(TAG, "1-Wire bus reset");
        return ESP_OK;
    }

    bool OneWireBus::isInitialized() const
    {
        return state_->isInitialized_;
    }

    bool OneWireBus::hasConfigChanged() const
    {
        OneWireBusConfig cfg{};
        state_->cfgStore_.getOneWireBusConfig(cfg);

        return (cfg != state_->initialCfg_);
    }

    esp_err_t OneWireBus::writeCmdPolling(uint64_t devAddr,
                                          const uint8_t *txCmd,
                                          uint8_t txCmdLen,
                                          uint32_t timeoutMillis)
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard lg(state_->lock_);

        ESP_RETURN_ON_FALSE(0 != devAddr, ESP_ERR_INVALID_ARG,
                            TAG, "Invalid device address");

        ESP_RETURN_ON_ERROR(
            state_->writeCmd(devAddr, txCmd, txCmdLen),
            TAG, "Failed write conversation");

        const auto POLLING_DELAY   = pdMS_TO_TICKS(10);
        const auto timeout_ticks   = pdMS_TO_TICKS(timeoutMillis);
        const auto starttime_ticks = xTaskGetTickCount();

        uint8_t ready = 0;

        while (0 == ready)
        {
            ESP_RETURN_ON_ERROR(
                onewire_bus_read_bit(state_->busHandle, &ready),
                TAG, "Failed polling");

            if (0 != ready)
                break;

            if ((xTaskGetTickCount() - starttime_ticks) > timeout_ticks)
                return ESP_ERR_TIMEOUT;

            vTaskDelay(POLLING_DELAY);
        }

        return ESP_OK;
    }

    esp_err_t OneWireBus::readData(uint64_t devAddr,
                                   const uint8_t *txCmd,
                                   uint8_t txCmdLen,
                                   uint8_t *rxBuf,
                                   size_t rxBufLen)
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard lg(state_->lock_);

        ESP_RETURN_ON_FALSE(0 != devAddr, ESP_ERR_INVALID_ARG,
                            TAG, "Invalid device address");

        ESP_RETURN_ON_ERROR(
            state_->writeCmd(devAddr, txCmd, txCmdLen),
            TAG, "Failed write conversation");

        ESP_RETURN_ON_ERROR(
            onewire_bus_read_bytes(state_->busHandle, rxBuf, rxBufLen),
            TAG, "Failed to read device data");

        return ESP_OK;
    }

    esp_err_t OneWireBus::writeData(uint64_t devAddr,
                                    const uint8_t *txCmd,
                                    uint8_t txCmdLen,
                                    const uint8_t *txBuf,
                                    uint8_t txBufLen)
    {

        configASSERT(state_->isInitialized_);
        std::lock_guard lg(state_->lock_);

        ESP_RETURN_ON_FALSE(0 != devAddr, ESP_ERR_INVALID_ARG,
                            TAG, "Invalid device address");

        ESP_RETURN_ON_ERROR(
            state_->writeCmd(devAddr, txCmd, txCmdLen),
            TAG, "Failed write conversation");

        ESP_RETURN_ON_ERROR(
            onewire_bus_write_bytes(state_->busHandle, txBuf, txBufLen),
            TAG, "Failed to write device data");

        return ESP_OK;
    }

    bool OneWireBus::verifyCrc8(uint8_t *buf, size_t bufLen) const
    {
        configASSERT(state_->isInitialized_);

        if (nullptr == buf || 2 > bufLen)
            return false;

        const uint8_t crc8 = onewire_crc8(0, buf, bufLen - 1);
        if (crc8 != buf[bufLen - 1])
        {
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, bufLen, ESP_LOG_ERROR);
            ESP_LOGE(TAG, "Invalid CRC8: calculated=0x%02X expected=0x%02X",
                     crc8, buf[bufLen - 1]);

            return false;
        }

        return true;
    }

    void OneWireBus::logConnectedDevices() const
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard lg(state_->lock_);

        onewire_device_iter_handle_t devIter = NULL;
        ESP_RETURN_VOID_ON_ERROR(
            onewire_new_device_iter(state_->busHandle, &devIter),
            TAG, "Failed to get 1-Wire device iterator");

        std::string logMsg;
        logMsg.reserve(128);

        onewire_device_t dev;
        while (ESP_OK == onewire_device_iter_get_next(devIter, &dev))
        {
            char buf[24];
            snprintf(buf, sizeof(buf), " 0x%016llX |", dev.address);
            logMsg.append(buf);
        }

        onewire_del_device_iter(devIter);
        ESP_LOGI(TAG, "Connected 1-Wire devices:%s",
                 logMsg.empty() ? " none" : logMsg.c_str());
    }

    bool OneWireBus::isDeviceAvailable(uint64_t devAddr) const
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard lg(state_->lock_);

        onewire_device_iter_handle_t devIter = NULL;
        if (ESP_OK != onewire_new_device_iter(state_->busHandle, &devIter))
        {
            ESP_LOGE(TAG, "Failed to get 1-Wire device iterator");
            return false;
        }

        const onewire_device_address_t addr =
            static_cast<onewire_device_address_t>(devAddr);
        bool found = false;
        onewire_device_t dev;
        while (ESP_OK == onewire_device_iter_get_next(devIter, &dev))
        {
            if (addr == dev.address)
            {
                found = true;
                break;
            }
        }

        onewire_del_device_iter(devIter);
        return found;
    }

    esp_err_t OneWireBus::State::writeCmd(uint64_t devAddr,
                                          const uint8_t *txCmd,
                                          uint8_t txCmdLen)
    {
        ESP_RETURN_ON_ERROR(
            onewire_bus_reset(busHandle),
            TAG, "No device available for addr=%016llX", devAddr);

        constexpr uint8_t match_cmd = ONEWIRE_CMD_MATCH_ROM;
        ESP_RETURN_ON_ERROR(
            onewire_bus_write_bytes(busHandle, &match_cmd, 1u),
            TAG, "Failed to write MATCH ROM");

        ESP_RETURN_ON_ERROR(
            onewire_bus_write_bytes(busHandle,
                                    reinterpret_cast<const uint8_t *>(&devAddr),
                                    8u),
            TAG, "Failed to write device address");

        ESP_RETURN_ON_ERROR(
            onewire_bus_write_bytes(busHandle, txCmd, txCmdLen),
            TAG, "Failed to write device data");

        return ESP_OK;
    }
}
