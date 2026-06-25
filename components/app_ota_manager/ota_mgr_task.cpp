#include "private_include/ota_mgr_internal.hpp"

namespace app::ota
{

    esp_err_t OtaManager::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&OtaManager::State::taskFunction,
                                     "ota_mgr_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void OtaManager::State::finiTask()
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

    void OtaManager::State::taskFunction(void *arg)
    {
        static_cast<OtaManager::State *>(arg)->runTask();
    }

    void OtaManager::State::runTask()
    {
        constexpr uint32_t TASK_WAIT_TIMEOUT_MS = 1000UL;

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

            if (0 != (bits & TaskWakeBit::START_OTA))
            {
                startOta();
            }
        }

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t OtaManager::State::initEvents()
    {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, app::sm::APP_EVENT_OTA_REQUEST,
                                &(otaEventHandler), (void *)this, &(otaEvtHdlr_)),
                            TAG, "Could not register event handler for IP");

        return ESP_OK;
    }

    void OtaManager::State::finiEvents()
    {
        if (nullptr != otaEvtHdlr_)
        {
            esp_event_handler_instance_unregister(app::sm::APP_EVENT,
                                                  app::sm::APP_EVENT_OTA_REQUEST,
                                                  otaEvtHdlr_);
            otaEvtHdlr_ = nullptr;
        }
    }

    void OtaManager::State::otaEventHandler(void *arg, esp_event_base_t eventBase,
                                            int32_t eventId, void *eventData)
    {
        auto *otaMgrState = static_cast<OtaManager::State *>(arg);
        otaMgrState->handleOtaEvent(eventBase, eventId, eventData);
    }

    void OtaManager::State::handleOtaEvent(const char *eventBase, int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

        switch (eventId)
        {
        case app::sm::APP_EVENT_OTA_REQUEST:
        {
            setOtaStatus(OtaState::REQUESTED);
            xEventGroupSetBits(eventGroup_, TaskWakeBit::START_OTA);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled APP event=%d", eventId);
        }
    }

    void OtaManager::State::startOta()
    {
        ESP_LOGI(TAG, "Starting OTA task");

        app::wm::WifiStatus wifiStatus{};
        stateMachine_.getWifiStatus(wifiStatus);

        if (!wifiStatus.sta_has_ip)
        {
            ESP_LOGE(TAG, "No Wifi connection available");
            setOtaStatus(OtaState::FAILED, ESP_ERR_INVALID_STATE);
            return;
        }

        app::ota::OtaManagerConfig otaCfg;
        cfgStore_.getOtaManagerConfig(otaCfg);

        if ('\0' == otaCfg.url[0])
        {
            ESP_LOGE(TAG, "No OTA url available");
            setOtaStatus(OtaState::FAILED, ESP_ERR_INVALID_ARG);
            return;
        }

        if (otaCfg.server_cert.empty())
        {
            ESP_LOGE(TAG, "No OTA cert available");
            setOtaStatus(OtaState::FAILED, ESP_ERR_INVALID_ARG);
            return;
        }

        setOtaStatus(OtaState::RUNNING);

        esp_http_client_config_t http_cfg = {};
        http_cfg.url                      = otaCfg.url;
        http_cfg.cert_pem                 = otaCfg.server_cert.c_str();

        esp_https_ota_config_t ota_config = {};
        ota_config.http_config            = &http_cfg;

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, esp_https_ota, (&ota_config),
                  "OTA image written successfully: URL=%s", http_cfg.url);

        if (ESP_OK == ret)
        {
            ret = stateMachine_.postEvent(app::sm::APP_EVENT,
                                          app::sm::APP_EVENT_RESTART_REQUEST);
            if (ESP_OK != ret)
            {
                ESP_LOGE(TAG, "Posting restart request failed: %s", esp_err_to_name(ret));
                setOtaStatus(OtaState::RESTART_PENDING, ret);
            }
            else
            {
                setOtaStatus(OtaState::RESTART_PENDING);
            }
        }
        else
        {
            setOtaStatus(OtaState::FAILED, ret);
        }
    }

    void OtaManager::State::setOtaStatus(OtaState otaState, esp_err_t last_error)
    {
        stateMachine_.updateOtaStatus(
            [otaState, last_error](auto &otaStatus)
            {
                otaStatus.state      = otaState;
                otaStatus.last_error = last_error;
            });
    }
}
