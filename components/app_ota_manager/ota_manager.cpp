#include "private_include/ota_mgr_internal.hpp"

namespace app::ota
{
    constexpr auto OM_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_OM_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < OM_LOG_LEVEL)
        {
            esp_log_level_set(TAG, OM_LOG_LEVEL);
        }
    }

    OtaManager::OtaManager(const app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
        : state_(std::make_unique<State>(cs, sm)) {}

    OtaManager::~OtaManager()
    {
        fini();
    }

    esp_err_t OtaManager::init()
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
        ESP_LOGD(TAG, "OTA Manager initialized");
        return ESP_OK;
    }

    void OtaManager::fini()
    {
        state_->finiEvents();
        state_->finiTask();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "OTA Manager finalized");
    }

    esp_err_t OtaManager::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset OTA Manager");

        ESP_LOGD(TAG, "OTA Manager reset");
        return ESP_OK;
    }

    bool OtaManager::isInitialized() const
    {
        return state_->isInitialized_;
    }
}