#include "esp_check.h"
#include "esp_event.h"
#include "app/cmn/mutex.hpp"
#include "app/sm/state_machine.hpp"

namespace app::sm
{
    constexpr auto TAG          = "app_state_machine";
    constexpr auto SM_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_SM_LOG_LEVEL);

    ESP_EVENT_DEFINE_BASE(APP_EVENT);

    void apply_log_level()
    {
        if (-1 < SM_LOG_LEVEL)
        {
            esp_log_level_set(TAG, SM_LOG_LEVEL);
        }
    }

    struct StateMachine::State
    {
        bool isInitialized_ = false;

        SystemStatus systemStatus_{};

        mutable app::cmn::Mutex mutexWifi_;
        mutable app::cmn::Mutex mutexOta_;
        mutable app::cmn::Mutex mutexMqtt_;
        mutable app::cmn::Mutex mutexSntp_;
        mutable app::cmn::Mutex mutexDevices_;
        mutable app::cmn::Mutex mutexBme280_;
        mutable app::cmn::Mutex mutexDs18b20_;
        mutable app::cmn::Mutex mutexTl136_;

        void lockAllMutexesOrdered();
        void unlockAllMutexesReverseOrdered();

        template <typename T>
        struct ComponentTraits;

        template <typename T>
        void updateComponentStatus(const std::function<void(T &)> &);

        template <typename T>
        void getComponentStatus(T &) const;
    };

    StateMachine::StateMachine()
        : state_(std::make_unique<State>()) {}

    StateMachine::~StateMachine()
    {
        fini();
    }

    esp_err_t StateMachine::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "State Machine initialized");
        return ESP_OK;
    }

    void StateMachine::updateWifiStatus(const std::function<void(app::wm::WifiStatus &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateOtaStatus(const std::function<void(app::ota::OtaStatus &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateMqttStatus(const std::function<void(app::mqtt::MqttClientStatus &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateSntpStatus(const std::function<void(app::sntp::SntpClientStatus &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateDevicesStatus(const std::function<void(app::dm::DeviceManagerStatus &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateBme280Status(const std::function<void(app::i2c::bme280::Bme280Status &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateDs18b20Status(const std::function<void(app::onewire::ds18b20::Ds18b20Status &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateTl136Status(const std::function<void(app::modbus::tl136::Tl136Status &)> &fn)
    {
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::fini()
    {
        state_->lockAllMutexesOrdered();
        state_->systemStatus_ = {};
        state_->unlockAllMutexesReverseOrdered();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "State Machine finalized");
    }

    esp_err_t StateMachine::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset State Machine");

        ESP_LOGD(TAG, "State Machine reset");
        return ESP_OK;
    }

    bool StateMachine::isInitialized() const
    {
        return state_->isInitialized_;
    }

    void StateMachine::getSnapshot(SystemStatus &systemStatus) const
    {
        configASSERT(state_->isInitialized_);
        state_->lockAllMutexesOrdered();
        systemStatus = state_->systemStatus_;
        state_->unlockAllMutexesReverseOrdered();
    }

    void StateMachine::getWifiStatus(app::wm::WifiStatus &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getOtaStatus(app::ota::OtaStatus &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getMqttStatus(app::mqtt::MqttClientStatus &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getSntpStatus(app::sntp::SntpClientStatus &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getDevicesStatus(app::dm::DeviceManagerStatus &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getBme280Status(app::i2c::bme280::Bme280Status &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getDs18b20Status(app::onewire::ds18b20::Ds18b20Status &status) const
    {
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getTl136Status(app::modbus::tl136::Tl136Status &status) const
    {
        state_->getComponentStatus<>(status);
    }

    esp_err_t StateMachine::postEvent(esp_event_base_t event_base, int32_t event_id)
    {
        configASSERT(state_->isInitialized_);
        return esp_event_post(event_base, event_id, nullptr, 0, portMAX_DELAY);
    }

    void StateMachine::State::lockAllMutexesOrdered()
    {
        mutexWifi_.lock();
        mutexOta_.lock();
        mutexMqtt_.lock();
        mutexSntp_.lock();
        mutexDevices_.lock();
        mutexBme280_.lock();
        mutexDs18b20_.lock();
        mutexTl136_.lock();
    }

    void StateMachine::State::unlockAllMutexesReverseOrdered()
    {
        mutexTl136_.unlock();
        mutexDs18b20_.unlock();
        mutexBme280_.unlock();
        mutexDevices_.unlock();
        mutexSntp_.unlock();
        mutexMqtt_.unlock();
        mutexOta_.unlock();
        mutexWifi_.unlock();
    }

    template <>
    struct StateMachine::State::ComponentTraits<app::wm::WifiStatus>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexWifi_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.wifi;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.wifi;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::ota::OtaStatus>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexOta_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.ota;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.ota;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::mqtt::MqttClientStatus>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexMqtt_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.mqtt;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.mqtt;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::sntp::SntpClientStatus>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexSntp_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.sntp;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.sntp;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::dm::DeviceManagerStatus>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexDevices_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.devices;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.devices;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::i2c::bme280::Bme280Status>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexBme280_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.bme280;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.bme280;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::onewire::ds18b20::Ds18b20Status>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexDs18b20_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.ds18b20;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.ds18b20;
        }
    };

    template <>
    struct StateMachine::State::ComponentTraits<app::modbus::tl136::Tl136Status>
    {
        static auto &mutex(const StateMachine::State &smState)
        {
            return smState.mutexTl136_;
        }

        static auto &status(SystemStatus &systemStatus)
        {
            return systemStatus.tl136;
        }

        static const auto &status(const SystemStatus &systemStatus)
        {
            return systemStatus.tl136;
        }
    };

    template <typename T>
    void StateMachine::State::updateComponentStatus(const std::function<void(T &)> &fn)
    {
        configASSERT(isInitialized_);
        std::lock_guard lg(ComponentTraits<T>::mutex(*this));
        configASSERT(fn);
        fn(ComponentTraits<T>::status(this->systemStatus_));
    }

    template <typename T>
    void StateMachine::State::getComponentStatus(T &status) const
    {
        configASSERT(isInitialized_);
        std::lock_guard lg(ComponentTraits<T>::mutex(*this));
        status = ComponentTraits<T>::status(this->systemStatus_);
    }
}
