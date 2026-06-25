#include "private_include/sntp_clt_internal.hpp"

namespace app::sntp
{
    constexpr auto SNTP_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_SNTP_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < SNTP_LOG_LEVEL)
        {
            esp_log_level_set(TAG, SNTP_LOG_LEVEL);
        }
    }

    SntpClient::SntpClient(const app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
        : state_(std::make_unique<State>(cs, sm)) {}

    SntpClient::~SntpClient()
    {
        fini();
    }

    esp_err_t SntpClient::init()
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
        ESP_LOGD(TAG, "SNTP Client initialized");
        return ESP_OK;
    }

    void SntpClient::fini()
    {
        state_->finiEvents();
        state_->finiTask();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "SNTP Client finalized");
    }

    esp_err_t SntpClient::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset SNTP Client");

        ESP_LOGD(TAG, "SNTP Client reset");
        return ESP_OK;
    }

    bool SntpClient::isInitialized() const
    {
        return state_->isInitialized_;
    }
}