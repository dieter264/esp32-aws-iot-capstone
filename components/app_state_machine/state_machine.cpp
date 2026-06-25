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
        SystemStatus systemStatus_{};

        EventGroupHandle_t eventGroup_{};

        mutable app::cmn::Mutex mutexWifi_;
        mutable app::cmn::Mutex mutexOta_;
        mutable app::cmn::Mutex mutexBme280_;
        mutable app::cmn::Mutex mutexMqtt_;
        mutable app::cmn::Mutex mutexSntp_;

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
        if (isInitialized_) return ESP_OK;

        state_->eventGroup_ = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(nullptr != state_->eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        isInitialized_ = true;
        ESP_LOGD(TAG, "State Machine initialized");
        return ESP_OK;
    }

    void StateMachine::updateWifiStatus(const std::function<void(app::wm::WifiStatus &)> &fn)
    {
        configASSERT(isInitialized_);
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateOtaStatus(const std::function<void(app::ota::OtaStatus &)> &fn)
    {
        configASSERT(isInitialized_);
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateBme280Status(const std::function<void(app::i2c::bme280::Bme280Status &)> &fn)
    {
        configASSERT(isInitialized_);
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateMqttStatus(const std::function<void(app::mqtt::MqttClientStatus &)> &fn)
    {
        configASSERT(isInitialized_);
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::updateSntpStatus(const std::function<void(app::sntp::SntpClientStatus &)> &fn)
    {
        configASSERT(isInitialized_);
        state_->updateComponentStatus<>(fn);
    }

    void StateMachine::fini()
    {
        state_->lockAllMutexesOrdered();
        state_->systemStatus_ = {};
        state_->unlockAllMutexesReverseOrdered();

        if (nullptr != state_->eventGroup_)
        {
            vEventGroupDelete(state_->eventGroup_);
            state_->eventGroup_ = {};
        }

        if (!isInitialized_) return;
        isInitialized_ = false;
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
        return isInitialized_;
    }

    void StateMachine::getSnapshot(SystemStatus &systemStatus) const
    {
        configASSERT(isInitialized_);
        state_->lockAllMutexesOrdered();
        systemStatus = state_->systemStatus_;
        state_->unlockAllMutexesReverseOrdered();
    }

    void StateMachine::getWifiStatus(app::wm::WifiStatus &status) const
    {
        configASSERT(isInitialized_);
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getOtaStatus(app::ota::OtaStatus &status) const
    {
        configASSERT(isInitialized_);
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getBme280Status(app::i2c::bme280::Bme280Status &status) const
    {
        configASSERT(isInitialized_);
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getMqttStatus(app::mqtt::MqttClientStatus &status) const
    {
        configASSERT(isInitialized_);
        state_->getComponentStatus<>(status);
    }

    void StateMachine::getSntpStatus(app::sntp::SntpClientStatus &status) const
    {
        configASSERT(isInitialized_);
        state_->getComponentStatus<>(status);
    }

    esp_err_t StateMachine::postEvent(esp_event_base_t event_base, int32_t event_id)
    {
        configASSERT(isInitialized_);
        return esp_event_post(event_base, event_id, nullptr, 0, portMAX_DELAY);
    }

    void StateMachine::State::lockAllMutexesOrdered()
    {
        mutexWifi_.lock();
        mutexSntp_.lock();
        mutexMqtt_.lock();
        mutexBme280_.lock();
        mutexOta_.lock();
    }

    void StateMachine::State::unlockAllMutexesReverseOrdered()
    {
        mutexOta_.unlock();
        mutexBme280_.unlock();
        mutexMqtt_.unlock();
        mutexSntp_.unlock();
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

    template <typename T>
    void StateMachine::State::updateComponentStatus(const std::function<void(T &)> &fn)
    {
        std::lock_guard lg(ComponentTraits<T>::mutex(*this));
        configASSERT(fn);
        fn(ComponentTraits<T>::status(this->systemStatus_));
    }

    template <typename T>
    void StateMachine::State::getComponentStatus(T &status) const
    {
        std::lock_guard lg(ComponentTraits<T>::mutex(*this));
        status = ComponentTraits<T>::status(this->systemStatus_);
    }
}
