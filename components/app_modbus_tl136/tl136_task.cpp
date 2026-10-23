#include "private_include/tl136_internal.hpp"

namespace app::modbus::tl136
{
    esp_err_t Tl136::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&Tl136::State::taskFunction,
                                     "tl136_sensor_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void Tl136::State::finiTask()
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

    void Tl136::State::taskFunction(void *arg)
    {
        static_cast<Tl136::State *>(arg)->runTask();
    }

    void Tl136::State::runTask()
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

            uint16_t mAx100{};
            auto ret = r4cva02Driver_->getCurrent_mAx100(mAx100);

            if (ESP_OK == ret)
            {
                ret = compensate(mAx100);
            }

            app::sntp::SntpClientStatus sntpStatus{};
            stateMachine_.getSntpStatus(sntpStatus);
            const auto isTimeValid     = sntpStatus.is_time_valid;
            const auto sampleTimeUtcUs = isTimeValid ? app::cmn::time::getUtcTimeUs() : 0ULL;

            stateMachine_.updateTl136Status(
                [this, isTimeValid, sampleTimeUtcUs, ret](auto &tl136Status)
                {
                    tl136Status.error_status = ret;

                    if (ESP_OK == ret)
                    {
                        tl136Status.data               = this->tl136Data_;
                        tl136Status.sample_time_utc_us = sampleTimeUtcUs;
                        tl136Status.sample_time_valid  = isTimeValid;
                    }
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_TL136_DATA_READY);
        }

        stateMachine_.updateTl136Status(
            [](auto &tl136Status)
            {
                tl136Status = {};
            });

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t Tl136::State::compensate(uint16_t mAx100)
    {
        constexpr uint16_t mAx100_min = 400;
        constexpr uint16_t mAx100_max = 2000;

        constexpr uint16_t mAx100_min_tolerated = 0.95 * mAx100_min;
        constexpr uint16_t mAx100_max_tolerated = 1.05 * mAx100_max;

        ESP_RETURN_ON_FALSE(
            (mAx100_min_tolerated <= mAx100) && (mAx100_max_tolerated >= mAx100),
            ESP_ERR_INVALID_ARG, TAG,
            "Value mAx100=%u outside range min=%u .. max=%u",
            mAx100, mAx100_min_tolerated, mAx100_max_tolerated);

        tl136Data_.current_mAx100 = mAx100;

        mAx100 = std::max(mAx100_min, mAx100);
        mAx100 = std::min(mAx100_max, mAx100);

        tl136Data_.water_level_mm =
            (uint32_t(mAx100 - mAx100_min) * initialCfg_.full_scale_water_level_mm) /
            (mAx100_max - mAx100_min);

        ESP_LOGI(TAG, "TL136 data:\n"
                      "  current mAx100=%" PRIu16 "\n"
                      "  water level mm=%" PRIu16,
                 tl136Data_.current_mAx100,
                 tl136Data_.water_level_mm);

        return ESP_OK;
    }
}
