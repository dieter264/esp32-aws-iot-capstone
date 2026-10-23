#include "private_include/bme280_internal.hpp"

namespace app::i2c::bme280
{
    esp_err_t Bme280::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&Bme280::State::taskFunction,
                                     "bme280_sensor_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void Bme280::State::finiTask()
    {
        if (nullptr != taskHandle_)
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP);

            const EventBits_t bits = xEventGroupWaitBits(
                eventGroup_,
                TaskWakeBit::STOP_ACK,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(app::cfg::TASK_SHUTDOWN_TIMEOUT_MS));

            if (0 == (bits & TaskWakeBit::STOP_ACK))
            {
                // Forced deletion could leave ESP-IDF resources in use. Preserve
                // the handle and event group so initTask() cannot create a second task.
                ESP_LOGE(TAG, "Task did not stop within timeout; keeping task resources alive");
                return;
            }

            taskHandle_ = nullptr;
        }

        if (nullptr != eventGroup_)
        {
            vEventGroupDelete(eventGroup_);
            eventGroup_ = nullptr;
        }
    }

    void Bme280::State::taskFunction(void *arg)
    {
        static_cast<Bme280::State *>(arg)->runTask();
    }

    void Bme280::State::runTask()
    {
        const uint32_t TASK_WAIT_TIMEOUT_MS = initialCfg_.probe_interval_ms;

        for (;;)
        {
            const EventBits_t bits = xEventGroupWaitBits(
                eventGroup_,
                TASK_WAIT_BIT_MASK,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(TASK_WAIT_TIMEOUT_MS));

            if (0 != (bits & TaskWakeBit::STOP))
            {
                break;
            }

            int32_t adcT, adcP, adcH;
            const auto ret = burstReadAdc(adcT, adcP, adcH);
            if (ESP_OK == ret)
            {
                compensate(adcT, adcP, adcH);
            }

            app::sntp::SntpClientStatus sntpStatus{};
            stateMachine_.getSntpStatus(sntpStatus);
            const auto isTimeValid     = sntpStatus.is_time_valid;
            const auto sampleTimeUtcUs = isTimeValid ? app::cmn::time::getUtcTimeUs() : 0ULL;

            stateMachine_.updateBme280Status(
                [this, isTimeValid, sampleTimeUtcUs, ret](auto &bme280Status)
                {
                    bme280Status.error_status = ret;

                    if (ESP_OK == ret)
                    {
                        bme280Status.data               = this->bme280Data_;
                        bme280Status.sample_time_utc_us = sampleTimeUtcUs;
                        bme280Status.sample_time_valid  = isTimeValid;
                    }
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_BME280_DATA_READY);
        }

        stateMachine_.updateBme280Status(
            [](auto &status)
            {
                status = {};
            });

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t Bme280::State::burstReadAdc(int32_t &adcT, int32_t &adcP, int32_t &adcH)
    {
        app::i2c::bus::I2cBusConfig i2cBusCfg{};
        cfgStore_.getI2cBusConfig(i2cBusCfg);

        uint8_t regAdc8 = 0xF7;
        uint8_t adc8[8] = {0};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit_receive,
                             (devHandle_, &regAdc8, 1, adc8, sizeof(adc8), i2cBusCfg.xfer_timeout_ms),
                             "Raw data found at 0x%02x", regAdc8);

        uint32_t adcP20 = (static_cast<uint32_t>(adc8[0]) << 12) |
                          (static_cast<uint32_t>(adc8[1]) << 4) |
                          (static_cast<uint32_t>(adc8[2]) >> 4);

        uint32_t adcT20 = (static_cast<uint32_t>(adc8[3]) << 12) |
                          (static_cast<uint32_t>(adc8[4]) << 4) |
                          (static_cast<uint32_t>(adc8[5]) >> 4);

        uint16_t adcH16 = (static_cast<uint16_t>(adc8[6]) << 8) |
                          static_cast<uint16_t>(adc8[7]);

        adcP = static_cast<int32_t>(adcP20);
        adcT = static_cast<int32_t>(adcT20);
        adcH = static_cast<int32_t>(adcH16);

        return ESP_OK;
    }

    void Bme280::State::compensate(const int32_t &adc_T, const int32_t &adc_P, const int32_t &adc_H)
    {
        const int32_t &T1 = calib_.T1;
        const int32_t &T2 = calib_.T2;
        const int32_t &T3 = calib_.T3;

        int32_t var1 = ((((adc_T >> 3) - (T1 << 1)) * T2) >> 11);
        int32_t var2 = (((((adc_T >> 4) - T1) * ((adc_T >> 4) - T1)) >> 12) * T3) >> 14;

        const int32_t t_fine = var1 + var2;

        bme280Data_.temp_degCx100 = (t_fine * 5 + 128) >> 8;

        const int64_t &P1 = calib_.P1;
        const int64_t &P2 = calib_.P2;
        const int64_t &P3 = calib_.P3;
        const int64_t &P4 = calib_.P4;
        const int64_t &P5 = calib_.P5;
        const int64_t &P6 = calib_.P6;
        const int64_t &P7 = calib_.P7;
        const int64_t &P8 = calib_.P8;
        const int64_t &P9 = calib_.P9;

        int64_t var1p = static_cast<int64_t>(t_fine) - 128000LL;
        int64_t var2p = var1p * var1p * P6;
        var2p         = var2p + ((var1p * P5) << 17);
        var2p         = var2p + (P4 << 35);
        var1p         = ((var1p * var1p * P3) >> 8) + ((var1p * P2) << 12);
        var1p         = (((static_cast<int64_t>(1) << 47) + var1p) * P1) >> 33;

        if (var1p != 0)
        { // avoid exception caused by division by zero

            int64_t p = 1048576LL - static_cast<int64_t>(adc_P);
            p         = (((p << 31) - var2p) * 3125) / var1p;
            var1p     = (P9 * (p >> 13) * (p >> 13)) >> 25;
            var2p     = (P8 * p) >> 19;

            bme280Data_.press_PaQ24d8 = static_cast<uint32_t>(((p + var1p + var2p) >> 8) + (P7 << 4));
        }

        const int32_t &H1 = calib_.H1;
        const int32_t &H2 = calib_.H2;
        const int32_t &H3 = calib_.H3;
        const int32_t &H4 = calib_.H4;
        const int32_t &H5 = calib_.H5;
        const int32_t &H6 = calib_.H6;

        int32_t varh        = t_fine - 76800;
        const int32_t var1h = (((adc_H << 14) - (H4 << 20) - (H5 * varh)) + 16384) >> 15;
        const int32_t var2h = ((((varh * H6) >> 10) * (((varh * H3) >> 11) + 32768)) >> 10) + 2097152;
        varh                = var1h * ((var2h * H2 + 8192) >> 14);
        varh                = varh - (((((varh >> 15) * (varh >> 15)) >> 7) * H1) >> 4);
        varh                = (varh < 0) ? 0 : varh;
        varh                = (varh > 419430400) ? 419430400 : varh;

        bme280Data_.hum_RhQ22d10 = static_cast<uint32_t>(varh >> 12);

        ESP_LOGI(TAG, "BME280 data:\n"
                      "  degC=%4d.%02d\n"
                      "  hPa =%4d.%02d\n"
                      "   RH =%4d.%02d%%",
                 bme280Data_.temp_degCx100 / 100, abs(bme280Data_.temp_degCx100) % 100,
                 (bme280Data_.press_PaQ24d8 >> 8) / 100, (bme280Data_.press_PaQ24d8 >> 8) % 100,
                 bme280Data_.hum_RhQ22d10 >> 10, ((bme280Data_.hum_RhQ22d10 & 0x3ff) * 100) >> 10);
    }
}
