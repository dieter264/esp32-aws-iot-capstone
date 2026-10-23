#include "private_include/ds18b20_internal.hpp"

namespace app::onewire::ds18b20
{
    esp_err_t Ds18b20::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&Ds18b20::State::taskFunction,
                                     "ds18b20_sensor_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void Ds18b20::State::finiTask()
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

    void Ds18b20::State::taskFunction(void *arg)
    {
        static_cast<Ds18b20::State *>(arg)->runTask();
    }

    void Ds18b20::State::runTask()
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

            uint8_t buf[16]{};
            auto ret = readRawData(buf, sizeof(buf));
            if (ESP_OK == ret)
            {
                ret = compensate(buf, sizeof(buf));
            }

            app::sntp::SntpClientStatus sntpStatus{};
            stateMachine_.getSntpStatus(sntpStatus);
            const auto isTimeValid     = sntpStatus.is_time_valid;
            const auto sampleTimeUtcUs = isTimeValid ? app::cmn::time::getUtcTimeUs() : 0ULL;

            stateMachine_.updateDs18b20Status(
                [this, isTimeValid, sampleTimeUtcUs, ret](auto &ds18b20Status)
                {
                    ds18b20Status.error_status = ret;

                    if (ESP_OK == ret)
                    {
                        ds18b20Status.data               = this->ds18b20Data_;
                        ds18b20Status.sample_time_utc_us = sampleTimeUtcUs;
                        ds18b20Status.sample_time_valid  = isTimeValid;
                    }
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_DS18B20_DATA_READY);
        }

        stateMachine_.updateDs18b20Status(
            [](auto &status)
            {
                status = {};
            });

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t Ds18b20::State::readRawData(uint8_t *buf, size_t bufLen)
    {
        configASSERT(DS18B20_READ_SCRATCHPAD_SIZE <= bufLen);
        memset(buf, 0, bufLen);

        ESP_RETURN_ON_ERROR(
            onewireBus_.writeCmdPolling(initialCfg_.address,
                                        &ONEWIRE_CMD_DS18B20_CONVERT_T, 1u),
            TAG, "Failed to 'Convert T'");

        ESP_RETURN_ON_ERROR(
            onewireBus_.readData(initialCfg_.address,
                                 &ONEWIRE_CMD_DS18B20_READ_SCRATCHPAD, 1u,
                                 buf, DS18B20_READ_SCRATCHPAD_SIZE),
            TAG, "Failed to 'Read Scratchpad'");

        ESP_RETURN_ON_FALSE(onewireBus_.verifyCrc8(buf, DS18B20_READ_SCRATCHPAD_SIZE),
                            ESP_ERR_INVALID_CRC, TAG, "Failed CRC8 verification");

        return ESP_OK;
    }

    esp_err_t Ds18b20::State::compensate(const uint8_t *buf, size_t bufLen)
    {
        configASSERT(DS18B20_READ_SCRATCHPAD_SIZE <= bufLen);

        int16_t rawTemp =
            static_cast<int16_t>((static_cast<uint16_t>(buf[1]) << 8) | buf[0]);
        ds18b20Data_.temp_degCx100 = rawTemp * TEMP_INC_12BIT_X100;

        ESP_LOGI(TAG, "D18B20 data:\n"
                      "  degC=%4d.%02d",
                 ds18b20Data_.temp_degCx100 / 100, abs(ds18b20Data_.temp_degCx100) % 100);

        return ESP_OK;
    }
}
