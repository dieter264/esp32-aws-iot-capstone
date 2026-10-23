#include "private_include/device_mgr_internal.hpp"

namespace app::dm
{
    template <class T>
    void finiAndDeleteItem(T *&item)
    {
        if (nullptr == item)
            return;

        item->fini();
        delete item;
        item = nullptr;
    }

    template <class Tbus>
    void resetBusRuntime(Tbus &busRuntime)
    {
        finiAndDeleteItem(busRuntime.bus);
    }

    template <class T>
    esp_err_t checkAndInitItem(T *&item, const char *itemName)
    {
        if (nullptr == item)
        {
            ESP_LOGE(TAG, "Failed to create '%s'", itemName);
            return ESP_ERR_NO_MEM;
        }
        const esp_err_t ret = item->init();
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to init '%s': %s", itemName, esp_err_to_name(ret));
            finiAndDeleteItem(item);
            return ret;
        }
        return ESP_OK;
    }

    template <class Tbus>
    esp_err_t DeviceManager::State::Runtime::checkBusAvailable(Tbus &busRt)
    {
        if (nullptr != busRt.bus)
            return ESP_OK;
        busRt.bus = new Tbus::BusType(cfgStore_);
        return checkAndInitItem(busRt.bus, busRt.busName);
    }

    template <class Tdev>
    void resetDeviceRuntime(Tdev &devRt)
    {
        devRt.retryIterMillis = {};
        devRt.retryStartTicks = {};
        finiAndDeleteItem(devRt.device);
    }

    void DeviceManager::State::Runtime::activateDevices()
    {
        cfgStore_.getDeviceManagerConfig(configured_);

        if (ESP_OK == activateBus(i2c_))
        {
            activateDevice(i2c_, bme280_);
        }

        if (ESP_OK == activateBus(onewire_))
        {
            activateDevice(onewire_, ds18b20_);
        }

        if (ESP_OK == activateBus(modbus_))
        {
            activateDevice(modbus_, tl136_);
        }

        updateStateMachine();
    }

    template <class Tbus>
    esp_err_t DeviceManager::State::Runtime::activateBus(Tbus &busRt)
    {
        const bool configured = (0 != (configured_.deviceFlags & busRt.devMask));

        if (!configured)
        {
            deactivateDevices(busRt.devMask);
            return ESP_OK;
        }

        if (nullptr != busRt.bus && busRt.bus->hasConfigChanged())
        {
            deactivateDevices(busRt.devMask);
        }

        return checkBusAvailable(busRt);
    }

    template <class Tbus, class Tdev>
    void DeviceManager::State::Runtime::activateDevice(Tbus &busRt, Tdev &devRt)
    {
        const bool configured = configured_.hasFlag(devRt.deviceID);
        const bool active     = active_.hasFlag(devRt.deviceID);

        if (!configured)
        {
            resetDeviceRuntime(devRt);
            active_.rmFlag(devRt.deviceID);
        }
        else if (!active || (nullptr != devRt.device && devRt.device->hasConfigChanged()))
        {
            resetDeviceRuntime(devRt);
            active_.rmFlag(devRt.deviceID);

            devRt.device = new Tdev::DeviceType(cfgStore_, stateMachine_, *busRt.bus);
            if (ESP_OK == checkAndInitItem(devRt.device, devRt.deviceName))
            {
                active_.setFlag(devRt.deviceID);
            }
        }
    }

    void DeviceManager::State::Runtime::monitorDevices()
    {
        statefulLogConnectedDevices();

        monitorDevice(i2c_, bme280_);
        monitorDevice(onewire_, ds18b20_);
        monitorDevice(modbus_, tl136_);

        if (active_ != lastActive_)
        {
            updateStateMachine();
        }
        else if (configured_ == lastConfigured_)
        {
            if (!isFirstDeviceLog_)
                return;
        }

        lastConfigured_   = configured_;
        lastActive_       = active_;
        isFirstDeviceLog_ = false;

        infoLogDevices();
    }

    template <class Tbus, class Tdev>
    void DeviceManager::State::Runtime::monitorDevice(Tbus &busRt, Tdev &devRt)
    {
        if (!configured_.hasFlag(devRt.deviceID))
            return;

        if ((nullptr != devRt.device) && (devRt.device->isInitialized()))
        {
            active_.setFlag(devRt.deviceID);
            devRt.retryIterMillis = 0;
            return;
        }

        active_.rmFlag(devRt.deviceID);
        if (0 == devRt.retryIterMillis)
        {
            devRt.retryIterMillis = 1000lu;
            devRt.retryStartTicks = xTaskGetTickCount();
        }

        const auto retMs = devRt.retryIterMillis;
        if ((xTaskGetTickCount() - devRt.retryStartTicks) >= pdMS_TO_TICKS(retMs))
        {
            if (nullptr != devRt.device)
            {
                if (ESP_OK == devRt.device->reset())
                {
                    active_.setFlag(devRt.deviceID);
                    devRt.retryIterMillis = 0;
                    return;
                }
            }
            else
            {
                activateDevice(busRt, devRt);
            }

            devRt.retryIterMillis = std::min(retMs * 2u, 600000lu);
            devRt.retryStartTicks = xTaskGetTickCount();
        }
    }

    void DeviceManager::State::Runtime::deactivateDevices(uint32_t devMask)
    {
        deactivateDevice(bme280_, devMask);
        deactivateDevice(ds18b20_, devMask);
        deactivateDevice(tl136_, devMask);

        deactivateBus(i2c_, devMask);
        deactivateBus(onewire_, devMask);
        deactivateBus(modbus_, devMask);

        updateStateMachine();
    }

    template <class Tdev>
    void DeviceManager::State::Runtime::deactivateDevice(Tdev &devRt, uint32_t devMask)
    {
        if (0 == (devMask & devRt.deviceID))
            return;

        resetDeviceRuntime(devRt);
        active_.rmFlag(devRt.deviceID);
    }

    template <class Tbus>
    void DeviceManager::State::Runtime::deactivateBus(Tbus &busRt, uint32_t devMask)
    {
        if (0 == (devMask & busRt.devMask))
            return;

        resetBusRuntime(busRt);
    }

    void DeviceManager::State::Runtime::updateStateMachine()
    {
        stateMachine_.updateDevicesStatus(
            [active = active_](auto &status)
            {
                status = active;
            });
    }

    void DeviceManager::State::Runtime::infoLogDevices() const
    {
        std::string logMsg;
        logMsg.reserve(128);

        const auto append = [this, &logMsg](const auto &dev)
        {
            if (!configured_.hasFlag(dev.deviceID))
                return;

            char buf[16];
            snprintf(buf, sizeof(buf), " %.8s (%u) |",
                     dev.deviceName, active_.hasFlag(dev.deviceID) ? 1u : 0u);

            logMsg.append(buf);
        };

        append(bme280_);
        append(ds18b20_);
        append(tl136_);

        ESP_LOGI(TAG, "Configured devices:%s",
                 logMsg.empty() ? " none" : logMsg.c_str());
    }

    void DeviceManager::State::Runtime::statefulLogConnectedDevices()
    {
        if (statefulLogDone_)
            return;

        if (ESP_OK == checkBusAvailable(modbus_))
        {
            if (1u <= currentMbDevAddr_ && 247u >= currentMbDevAddr_)
            {
                const auto ret = modbus_.bus->scanDeviceAddress(currentMbDevAddr_);
                if (ESP_OK == ret)
                {
                    foundMbDevAddr_[currentMbDevAddr_] = 1;
                }
                currentMbDevAddr_++;
                return;
            }

            std::string logMsg;
            logMsg.reserve(128);

            size_t i = foundMbDevAddr_._Find_first();
            while (i < foundMbDevAddr_.size())
            {
                char buf[24];
                snprintf(buf, sizeof(buf), " %03" PRIu8 " |", uint8_t(i));
                logMsg.append(buf);

                i = foundMbDevAddr_._Find_next(i);
            }

            ESP_LOGI(TAG, "Connected Modbus devices:%s",
                     logMsg.empty() ? " none" : logMsg.c_str());
        }

        if (ESP_OK == checkBusAvailable(i2c_))
        {
            i2c_.bus->logConnectedDevices();
        }

        if (ESP_OK == checkBusAvailable(onewire_))
        {
            onewire_.bus->logConnectedDevices();
        }

        statefulLogDone_ = true;
    }

    uint32_t DeviceManager::State::Runtime::getTaskWaitToutMs()
    {
        constexpr uint32_t TASK_WAIT_TIMEOUT_MS = 1000UL;

        if (statefulLogDone_ || ESP_OK != checkBusAvailable(modbus_))
            return TASK_WAIT_TIMEOUT_MS;

        app::modbus::bus::ModbusBusConfig cfg{};
        cfgStore_.getModbusBusConfig(cfg);
        int cfgTout = cfg.response_tout_ms;

#ifdef CONFIG_FMB_MASTER_TIMEOUT_MS_RESPOND
        if (0 == cfg.response_tout_ms)
        {
            cfgTout = CONFIG_FMB_MASTER_TIMEOUT_MS_RESPOND;
        }
#endif

#ifdef CONFIG_FMB_MASTER_MAX_API_BLOCKING_TIME_MS
        cfgTout = std::min(CONFIG_FMB_MASTER_MAX_API_BLOCKING_TIME_MS, cfgTout);
#endif

        return uint32_t(std::max(0, int(TASK_WAIT_TIMEOUT_MS) - cfgTout));
    }

    void DeviceManager::State::Runtime::reset()
    {
        auto &cs = cfgStore_;
        auto &sm = stateMachine_;

        std::destroy_at(this);
        std::construct_at(this, cs, sm);
    }
}
