#include "private_include/device_mgr_internal.hpp"

namespace app::dm
{

    esp_err_t DeviceManager::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&DeviceManager::State::taskFunction,
                                     "device_mgr_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void DeviceManager::State::finiTask()
    {
        if (nullptr != taskHandle_)
        {
            DeviceManagerStatus status{};
            stateMachine_.getDevicesStatus(status);

            const auto numActDev     = static_cast<uint32_t>(std::popcount(status.deviceFlags));
            const uint32_t timeoutMs = (numActDev + 1u) * app::cfg::TASK_SHUTDOWN_TIMEOUT_MS;

            xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP);

            const EventBits_t bits = xEventGroupWaitBits(
                eventGroup_,
                TaskWakeBit::STOP_ACK,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(timeoutMs));

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

    void DeviceManager::State::taskFunction(void *arg)
    {
        static_cast<DeviceManager::State *>(arg)->runTask();
    }

    void DeviceManager::State::runTask()
    {
        runtime_.activateDevices();

        for (;;)
        {
            const EventBits_t bits = xEventGroupWaitBits(
                eventGroup_,
                TASK_WAIT_BIT_MASK,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(runtime_.getTaskWaitToutMs()));

            if (0 != (bits & TaskWakeBit::STOP))
            {
                break;
            }

            if (0 != (bits & TaskWakeBit::UPDATE_DEVICE))
            {
                runtime_.activateDevices();
            }

            runtime_.monitorDevices();
        }

        runtime_.deactivateDevices();
        runtime_.reset();

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t DeviceManager::State::initEvents()
    {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, app::sm::APP_EVENT_CONFIG_UPDATE,
                                &(deviceEventHandler), (void *)this, &(deviceEvtHdlr_)),
                            TAG, "Could not register event handler for IP");

        return ESP_OK;
    }

    void DeviceManager::State::finiEvents()
    {
        if (nullptr != deviceEvtHdlr_)
        {
            esp_event_handler_instance_unregister(app::sm::APP_EVENT,
                                                  app::sm::APP_EVENT_CONFIG_UPDATE,
                                                  deviceEvtHdlr_);
            deviceEvtHdlr_ = nullptr;
        }
    }

    void DeviceManager::State::deviceEventHandler(void *arg, esp_event_base_t eventBase,
                                                  int32_t eventId, void *eventData)
    {
        auto *deviceMgrState = static_cast<DeviceManager::State *>(arg);
        deviceMgrState->handleDeviceEvent(eventBase, eventId, eventData);
    }

    void DeviceManager::State::handleDeviceEvent(const char *eventBase, int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

        switch (eventId)
        {
        case app::sm::APP_EVENT_CONFIG_UPDATE:
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::UPDATE_DEVICE);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled APP event=%d", eventId);
        }
    }
}
