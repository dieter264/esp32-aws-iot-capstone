#include "private_include/ds18b20_internal.hpp"

namespace app::onewire::ds18b20
{
    constexpr auto DS18B20_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_ONEWIRE_DS18B20_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < DS18B20_LOG_LEVEL)
        {
            esp_log_level_set(TAG, DS18B20_LOG_LEVEL);
        }
    }

    Ds18b20::Ds18b20(app::cfg::ConfigStore &store,
                     app::sm::StateMachine &sm,
                     app::onewire::bus::OneWireBus &bus)
        : state_(std::make_unique<State>(store, sm, bus)) {}

    Ds18b20::~Ds18b20()
    {
        fini();
    }

    esp_err_t Ds18b20::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        ESP_RETURN_ON_FALSE(state_->onewireBus_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "1-Wire Bus not initialized");

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
        ESP_LOGD(TAG, "DS18B20 initialized");
        return ESP_OK;
    }

    void Ds18b20::fini()
    {
        state_->finiTask();
        state_->finiDevice();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "DS18B20 finalized");
    }

    esp_err_t Ds18b20::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset DS18B20");

        ESP_LOGD(TAG, "DS18B20 reset");
        return ESP_OK;
    }

    bool Ds18b20::isInitialized() const
    {
        return state_->isInitialized_;
    }

    bool Ds18b20::hasConfigChanged() const
    {
        Ds18b20Config cfg{};
        state_->cfgStore_.getDs18b20Config(cfg);

        return (cfg != state_->initialCfg_);
    }

    esp_err_t Ds18b20::State::initDevice()
    {
        cfgStore_.getDs18b20Config(initialCfg_);

        const uint8_t fam_code = static_cast<uint8_t>(initialCfg_.address & 0xFF);

        ESP_RETURN_ON_FALSE(DS18B20_FAMILY_CODE == fam_code, ESP_FAIL,
                            TAG, "Invalid 1-Wire ROM add=0x%016llX (missing family code 0x%0X)",
                            initialCfg_.address, DS18B20_FAMILY_CODE);

        ESP_RETURN_ON_FALSE(onewireBus_.isDeviceAvailable(initialCfg_.address), ESP_FAIL,
                            TAG, "No device found at addr=0x%016llX",
                            initialCfg_.address);

        ESP_LOGI(TAG, "DS18B20 available at addr=0x%016llX",
                 initialCfg_.address);

        uint8_t buf[16]{};

        ESP_RETURN_ON_ERROR(
            onewireBus_.readData(initialCfg_.address,
                                 &ONEWIRE_CMD_DS18B20_READ_SCRATCHPAD, 1u,
                                 buf, DS18B20_READ_SCRATCHPAD_SIZE),
            TAG, "Failed to 'Read Scratchpad'");

        ESP_RETURN_ON_FALSE(onewireBus_.verifyCrc8(buf, DS18B20_READ_SCRATCHPAD_SIZE),
                            ESP_ERR_INVALID_CRC, TAG, "Failed CRC8 verification");

        buf[4] |= DS18B20_12BIT_CONF;
        uint8_t conf[4]{};
        memcpy(conf, &buf[2], DS18B20_WRITE_SCRATCHPAD_SIZE);

        ESP_RETURN_ON_ERROR(
            onewireBus_.writeData(initialCfg_.address,
                                  &ONEWIRE_CMD_DS18B20_WRITE_SCRATCHPAD, 1u,
                                  conf, DS18B20_WRITE_SCRATCHPAD_SIZE),
            TAG, "Failed to 'Write Scratchpad'");

        ESP_RETURN_ON_ERROR(
            onewireBus_.readData(initialCfg_.address,
                                 &ONEWIRE_CMD_DS18B20_READ_SCRATCHPAD, 1u,
                                 buf, DS18B20_READ_SCRATCHPAD_SIZE),
            TAG, "Failed to 'Read Scratchpad'");

        ESP_RETURN_ON_FALSE(onewireBus_.verifyCrc8(buf, DS18B20_READ_SCRATCHPAD_SIZE),
                            ESP_ERR_INVALID_CRC, TAG, "Failed CRC8 verification");

        bool is12bit = DS18B20_12BIT_CONF == (buf[4] & DS18B20_12BIT_CONF);

        ESP_RETURN_ON_FALSE(is12bit, ESP_ERR_INVALID_STATE, TAG, "No 12-bit resolution");

        return ESP_OK;
    }

    void Ds18b20::State::finiDevice()
    {
        return void();
    }
}
