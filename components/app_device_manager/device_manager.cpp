#include "private_include/device_mgr_internal.hpp"

namespace app::dm
{
    constexpr auto DM_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_DM_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < DM_LOG_LEVEL)
        {
            esp_log_level_set(TAG, DM_LOG_LEVEL);
        }

        app::i2c::bus::apply_log_level();
        app::i2c::bme280::apply_log_level();
        app::onewire::bus::apply_log_level();
        app::onewire::ds18b20::apply_log_level();
        app::modbus::bus::apply_log_level();
        app::modbus::tl136::apply_log_level();
    }

    DeviceManager::DeviceManager(app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
        : state_(std::make_unique<State>(cs, sm)) {}

    DeviceManager::~DeviceManager()
    {
        fini();
    }

    esp_err_t DeviceManager::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, state_->initTask, (), "Task initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initEvents, (), "Event handling initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "Device Manager initialized");
        return ESP_OK;
    }

    void DeviceManager::fini()
    {
        state_->finiEvents();
        state_->finiTask();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "Device Manager finalized");
    }

    esp_err_t DeviceManager::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset Device Manager");

        ESP_LOGD(TAG, "Device Manager reset");
        return ESP_OK;
    }

    bool DeviceManager::isInitialized() const
    {
        return state_->isInitialized_;
    }
}
