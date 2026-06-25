#include "private_include/mqtt_clt_internal.hpp"

namespace app::mqtt
{
    constexpr auto MC_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_MC_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < MC_LOG_LEVEL)
        {
            esp_log_level_set(TAG, MC_LOG_LEVEL);
        }
    }

    MqttClient::MqttClient(const app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
        : state_(std::make_unique<State>(cs, sm)) {}

    MqttClient::~MqttClient()
    {
        fini();
    }

    esp_err_t MqttClient::init()
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
        ESP_LOGD(TAG, "MQTT Client initialized");
        return ESP_OK;
    }

    void MqttClient::fini()
    {
        state_->finiEvents();
        state_->finiTask();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "MQTT Client finalized");
    }

    esp_err_t MqttClient::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset MQTT Client");

        ESP_LOGD(TAG, "MQTT Client reset");
        return ESP_OK;
    }

    bool MqttClient::isInitialized() const
    {
        return state_->isInitialized_;
    }
}