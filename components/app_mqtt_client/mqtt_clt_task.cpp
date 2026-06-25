#include "private_include/mqtt_clt_internal.hpp"

namespace app::mqtt
{
    esp_err_t MqttClient::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&MqttClient::State::taskFunction,
                                     "mqtt_clt_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void MqttClient::State::finiTask()
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

    void MqttClient::State::taskFunction(void *arg)
    {
        static_cast<MqttClient::State *>(arg)->runTask();
    }

    void MqttClient::State::runTask()
    {
        constexpr uint32_t TASK_WAIT_TIMEOUT_MS = 5000UL;

        createAndStartClient();

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

            if (0 != (bits & TaskWakeBit::CONFIG_UPDATE))
            {
                app::mqtt::MqttClientConfig mqttCfg{};
                cfgStore_.getMqttClientConfig(mqttCfg);
                if (lastUsedMqttClientConfig_ != mqttCfg)
                {
                    ESP_LOGI(TAG, "MQTT configuration changed");
                    // Broker identity and TLS trust are initialization-time settings.
                    stopAndDestroyClient();
                    createAndStartClient();
                }
            }

            if (0 != (bits & TaskWakeBit::WIFI_UPDATE) || retryStartPending_)
            {
                app::wm::WifiStatus wifiStatus{};
                stateMachine_.getWifiStatus(wifiStatus);
                if (wifiStatus.sta_has_ip)
                {
                    startClient();
                }
                else
                {
                    stopClient();
                    retryStartPending_ = false;
                }
            }

            if (0 != (bits & TaskWakeBit::BME280_DATA_READY))
            {
                publishLatestBme280Data();
            }
        }

        stopAndDestroyClient();

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t MqttClient::State::initEvents()
    {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, ESP_EVENT_ANY_ID,
                                &(eventHandler), (void *)this, &(eventHandler_)),
                            TAG, "Could not register event handler for APP ANY");

        return ESP_OK;
    }

    void MqttClient::State::finiEvents()
    {
        if (nullptr != eventHandler_)
        {
            esp_event_handler_instance_unregister(app::sm::APP_EVENT, ESP_EVENT_ANY_ID,
                                                  eventHandler_);

            eventHandler_ = nullptr;
        }
    }

    void MqttClient::State::eventHandler(void *arg, esp_event_base_t eventBase,
                                         int32_t eventId, void *eventData)
    {
        auto *mqttClientState = static_cast<MqttClient::State *>(arg);
        mqttClientState->handleEvent(eventBase, eventId, eventData);
    }

    void MqttClient::State::handleEvent(const char *eventBase, int32_t eventId, void *)
    {
        ESP_LOGD(TAG, "Received event base=%s, id=%d", eventBase, eventId);

        switch (eventId)
        {
        case app::sm::APP_EVENT_CONFIG_UPDATE:
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::CONFIG_UPDATE);
        }
        break;

        case app::sm::APP_EVENT_WIFI_UPDATE:
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::WIFI_UPDATE);
        }
        break;

        case app::sm::APP_EVENT_BME280_DATA_READY:
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::BME280_DATA_READY);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled APP event=%d", eventId);
        }
    }
}
