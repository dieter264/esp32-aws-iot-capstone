#include "private_include/tl136_internal.hpp"

namespace app::modbus::tl136
{
    constexpr auto TL136_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_MODBUS_TL136_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < TL136_LOG_LEVEL)
        {
            esp_log_level_set(TAG, TL136_LOG_LEVEL);
        }
    }

    Tl136::Tl136(app::cfg::ConfigStore &store,
                 app::sm::StateMachine &sm,
                 app::modbus::bus::ModbusBus &bus)
        : state_(std::make_unique<State>(store, sm, bus)) {}

    Tl136::~Tl136()
    {
        fini();
    }

    esp_err_t Tl136::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        ESP_RETURN_ON_FALSE(state_->modebusBus_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Modebus Bus not initialized");

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, state_->initDevice, (), "Device initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initTask, (), "Task initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "TL136 initialized");
        return ESP_OK;
    }

    void Tl136::fini()
    {
        state_->finiTask();
        state_->finiDevice();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "TL136 finalized");
    }

    esp_err_t Tl136::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset TL136");

        ESP_LOGD(TAG, "TL136 reset");
        return ESP_OK;
    }

    bool Tl136::isInitialized() const
    {
        return state_->isInitialized_;
    }

    bool Tl136::hasConfigChanged() const
    {
        Tl136Config cfg{};
        state_->cfgStore_.getTl136Config(cfg);

        return (cfg != state_->initialCfg_);
    }

    esp_err_t Tl136::State::initDevice()
    {
        cfgStore_.getTl136Config(initialCfg_);

        r4cva02Driver_ = new R4cva02(cfgStore_, modebusBus_);
        configASSERT(nullptr != r4cva02Driver_);

        ESP_RETURN_ON_ERROR(r4cva02Driver_->init(), TAG,
                            "Failed to initialize R4CVA02 driver");
        return ESP_OK;
    }

    void Tl136::State::finiDevice()
    {
        if (nullptr != r4cva02Driver_)
        {
            r4cva02Driver_->fini();
            delete r4cva02Driver_;
            r4cva02Driver_ = {};
        }
    }
}
