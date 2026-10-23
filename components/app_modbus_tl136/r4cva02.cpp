#include "private_include/r4cva02.hpp"

#include <cinttypes>
#include <cstdint>
#include <memory>

#include "freertos/FreeRTOS.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "app/cfg/config_store.hpp"
#include "app/cfg/types.hpp"
#include "app/cmn/log.hpp"
#include "app/modbus/mb_bus.hpp"

namespace app::modbus::tl136
{
    constexpr auto TAG = "app_modbus_tl136";

    struct R4cva02::State
    {
        explicit State(app::cfg::ConfigStore &store,
                       app::modbus::bus::ModbusBus &bus)
            : cfgStore_(store),
              modbusBus_(bus) {}

        bool isInitialized_ = false;
        Tl136Config initialCfg_{};

        app::cfg::ConfigStore &cfgStore_;
        app::modbus::bus::ModbusBus &modbusBus_;

        esp_err_t initCalibration();
    };

    R4cva02::R4cva02(app::cfg::ConfigStore &store,
                     app::modbus::bus::ModbusBus &bus)
        : state_(std::make_unique<State>(store, bus)) {}

    R4cva02::~R4cva02()
    {
        fini();
    }

    esp_err_t R4cva02::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->modbusBus_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Modbus Bus not initialized");

        state_->cfgStore_.getTl136Config(state_->initialCfg_);

        esp_err_t ret = ESP_FAIL;

        LOGD_CALL(TAG, ret, state_->initCalibration, (), "R4CVA02 converter calibrated");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "R4CVA02 driver initialized");
        return ESP_OK;
    }

    void R4cva02::fini()
    {
        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "R4CVA02 driver finalized");
    }

    esp_err_t R4cva02::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset R4CVA02 driver");

        ESP_LOGD(TAG, "R4CVA02 driver reset");
        return ESP_OK;
    }

    bool R4cva02::isInitialized() const
    {
        return state_->isInitialized_;
    }

    esp_err_t R4cva02::getCurrent_mAx100(uint16_t &mAx100)
    {
        configASSERT(state_->isInitialized_);

        auto ret = state_->modbusBus_.readHoldingRegister(
            state_->initialCfg_.ai_converter_address,
            (1 == state_->initialCfg_.ai_converter_channel) ? 0xA0 : 0xA1,
            mAx100);

        ESP_RETURN_ON_FALSE(
            (ESP_OK == ret), ret, TAG,
            "Reading analog input from R4CVA02 converter failed: %s",
            esp_err_to_name(ret));

        return ESP_OK;
    }

    esp_err_t R4cva02::State::initCalibration()
    {
        auto ret = modbusBus_.writeHoldingRegister(
            initialCfg_.ai_converter_address,
            (1 == initialCfg_.ai_converter_channel) ? 0xC0 : 0xC1,
            initialCfg_.ai_converter_calibration);

        ESP_RETURN_ON_FALSE(
            (ESP_OK == ret), ret, TAG,
            "Calibration of R4CVA02 converter with %u failed: %s",
            initialCfg_.ai_converter_calibration, esp_err_to_name(ret));

        uint16_t calib{};

        ret = modbusBus_.readHoldingRegister(
            initialCfg_.ai_converter_address,
            (1 == initialCfg_.ai_converter_channel) ? 0xC0 : 0xC1,
            calib);

        ESP_RETURN_ON_FALSE(
            (ESP_OK == ret), ret, TAG,
            "Verifying R4CVA02 converter calibration failed: %s",
            esp_err_to_name(ret));

        ESP_RETURN_ON_FALSE(
            (calib == initialCfg_.ai_converter_calibration), ESP_ERR_INVALID_STATE, TAG,
            "Verifying calibration of R4CVA02 converter failed: expected %u, read %u",
            initialCfg_.ai_converter_calibration, calib);

        ESP_LOGI(TAG, "R4CVA02 converter calibrated with %" PRIu16, calib);
        return ESP_OK;
    }
}
