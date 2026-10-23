#include "private_include/wifi_mgr_internal.hpp"

namespace app::wm
{
    esp_err_t WifiManager::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&WifiManager::State::taskFunction,
                                     "wifi_mgr_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void WifiManager::State::finiTask()
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

    void WifiManager::State::taskFunction(void *arg)
    {
        static_cast<WifiManager::State *>(arg)->runTask();
    }

    static uint8_t getStatusBits(bool isTimeout, bool hasIP, bool isPendingStaCfgUpdate)
    {
        static constexpr uint8_t IS_TIMEOUT_BIT = 1u << 0;
        static constexpr uint8_t HAS_IP         = 1u << 1;
        static constexpr uint8_t IS_CFG_UPDATE  = 1u << 2;

        uint8_t bits = 0u;
        if (isTimeout) bits |= IS_TIMEOUT_BIT;
        if (hasIP) bits |= HAS_IP;
        if (isPendingStaCfgUpdate) bits |= IS_CFG_UPDATE;
        return bits;
    }

    void WifiManager::State::runTask()
    {
        constexpr uint32_t TASK_WAIT_TIMEOUT_MS     = 1000UL;
        constexpr uint32_t MIN_WIFI_WAIT_TIMEOUT_MS = 1000UL;
        constexpr uint32_t MAX_WIFI_WAIT_TIMEOUT_MS = 30000UL;

        TickType_t wait_timeout      = pdMS_TO_TICKS(MIN_WIFI_WAIT_TIMEOUT_MS);
        TickType_t was               = xTaskGetTickCount();
        uint8_t lastLoggedStatusBits = 0xffu;

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

            const bool isTimeout = (0 == (bits & TASK_WAIT_BIT_MASK));
            app::wm::WifiStatus wifiStatus{};
            stateMachine_.getWifiStatus(wifiStatus);
            const bool hasIP = wifiStatus.sta_has_ip;
            if ((0 != (bits & TaskWakeBit::CONFIG_UPDATED)) && isWifiStaConfigUpdate())
            {
                isPendingStaCfgUpdate_ = true;
            }

            uint8_t statusBits = getStatusBits(isTimeout, hasIP, isPendingStaCfgUpdate_);
            if (lastLoggedStatusBits != statusBits)
            {
                ESP_LOGD(TAG, "Status of WifiManger task:\n"
                              "       timeout= %s\n"
                              "        has IP= %s\n"
                              "sta cfg update= %s\n",
                         (isTimeout) ? "true" : "false", (hasIP) ? "true" : "false",
                         (isPendingStaCfgUpdate_) ? "true" : "false");

                lastLoggedStatusBits = statusBits;
            }

            if (hasIP && isTimeout)
            {
                wifi_ap_record_t apRec{};
                if ((ESP_OK == esp_wifi_sta_get_ap_info(&apRec)) &&
                    (wifiStatus.rssi != apRec.rssi))
                {
                    stateMachine_.updateWifiStatus(
                        [rssi = apRec.rssi](auto &wifiStatus)
                        {
                            wifiStatus.rssi = rssi;
                        });
                }
            }

            if (isPendingStaCfgUpdate_)
            {
                // Apply station credentials through a clean disconnect/reconnect cycle.
                if (hasIP)
                {
                    disconnectWifiSta();
                }
                else
                {
                    connectWifiSta();
                }
            }
            else if (!hasIP)
            {
                const TickType_t now = xTaskGetTickCount();
                if ((now - was) > wait_timeout)
                {
                    was          = now;
                    wait_timeout = std::min(wait_timeout * 2, pdMS_TO_TICKS(MAX_WIFI_WAIT_TIMEOUT_MS));
                    connectWifiSta();
                }
            }
            else
            {
                wait_timeout = pdMS_TO_TICKS(MIN_WIFI_WAIT_TIMEOUT_MS);
            }
        }

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t WifiManager::State::initEvents()
    {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                IP_EVENT, IP_EVENT_STA_GOT_IP,
                                &(ipEventHandler), (void *)this,
                                &(ipEventHandlerInstance_)),
                            TAG, "Could not register event handler for IP");

        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                WIFI_EVENT, ESP_EVENT_ANY_ID,
                                &(wifiEventHandler), (void *)this,
                                &(wifiEventHandlerInstance_)),
                            TAG, "Could not register event handler for WIFI");

        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, app::sm::APP_EVENT_CONFIG_UPDATE,
                                &(configEventHandler), (void *)this,
                                &(configEventHandlerInstance_)),
                            TAG, "Could not register event handler for CONFIG");

        return ESP_OK;
    }

    void WifiManager::State::finiEvents()
    {
        if (nullptr != configEventHandlerInstance_)
        {
            esp_event_handler_instance_unregister(app::sm::APP_EVENT,
                                                  app::sm::APP_EVENT_CONFIG_UPDATE,
                                                  configEventHandlerInstance_);
            configEventHandlerInstance_ = nullptr;
        }

        if (nullptr != wifiEventHandlerInstance_)
        {
            esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                  wifiEventHandlerInstance_);
            wifiEventHandlerInstance_ = nullptr;
        }

        if (nullptr != ipEventHandlerInstance_)
        {
            esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                  ipEventHandlerInstance_);
            ipEventHandlerInstance_ = nullptr;
        }
    }

    void WifiManager::State::ipEventHandler(void *arg, esp_event_base_t eventBase,
                                            int32_t eventId, void *eventData)
    {
        auto *wifiMgrState = static_cast<WifiManager::State *>(arg);
        wifiMgrState->handleIpEvent(eventBase, eventId, eventData);
    }

    void WifiManager::State::wifiEventHandler(void *arg, esp_event_base_t eventBase,
                                              int32_t eventId, void *eventData)
    {
        auto *wifiMgrState = static_cast<WifiManager::State *>(arg);
        wifiMgrState->handleWifiEvent(eventBase, eventId, eventData);
    }

    void WifiManager::State::configEventHandler(void *arg, esp_event_base_t eventBase,
                                                int32_t eventId, void *eventData)
    {
        auto *wifiMgrState = static_cast<WifiManager::State *>(arg);
        wifiMgrState->handleConfigEvent(eventBase, eventId, eventData);
    }

    void WifiManager::State::handleIpEvent(const char *eventBase, int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

        switch (eventId)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            auto *event = (ip_event_got_ip_t *)eventData;

            stateMachine_.updateWifiStatus(
                [event](auto &wifiStatus)
                {
                    wifiStatus.sta_has_ip = true;
                    snprintf(wifiStatus.sta_ip, sizeof(wifiStatus.sta_ip),
                             IPSTR, IP2STR(&event->ip_info.ip));
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_WIFI_UPDATE);
            xEventGroupSetBits(eventGroup_, TaskWakeBit::STA_GOT_IP);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled IP event=%d", eventId);
        }
    }

    void WifiManager::State::handleWifiEvent(const char *eventBase, int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

        switch (eventId)
        {
        case WIFI_EVENT_STA_CONNECTED:
        {
            wifi_ap_record_t apRec{};
            getApRecord(apRec, *(wifi_event_sta_connected_t *)eventData);

            stateMachine_.updateWifiStatus(
                [apRec](auto &wifiStatus)
                {
                    wifiStatus.sta_connected = true;
                    app::cfg::cstrCopy(wifiStatus.ssid, reinterpret_cast<const char *>(apRec.ssid));
                    wifiStatus.rssi                   = apRec.rssi;
                    wifiStatus.last_disconnect_reason = {};
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_WIFI_UPDATE);
        }
        break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            auto *event = (wifi_event_sta_disconnected_t *)eventData;

            stateMachine_.updateWifiStatus(
                [event](auto &wifiStatus)
                {
                    wifiStatus.sta_connected          = false;
                    wifiStatus.ssid[0]                = '\0';
                    wifiStatus.sta_has_ip             = false;
                    wifiStatus.sta_ip[0]              = '\0';
                    wifiStatus.rssi                   = RSSI_INVALID_DBM;
                    wifiStatus.last_disconnect_reason = event->reason;
                });

            stateMachine_.postEvent(app::sm::APP_EVENT, app::sm::APP_EVENT_WIFI_UPDATE);
            xEventGroupSetBits(eventGroup_, TaskWakeBit::STA_DISCONNECTED);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled WIFI event=%d", eventId);
        }
    }

    void WifiManager::State::handleConfigEvent(const char *eventBase, int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

        switch (eventId)
        {
        case app::sm::APP_EVENT_CONFIG_UPDATE:
        {
            xEventGroupSetBits(eventGroup_, TaskWakeBit::CONFIG_UPDATED);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled CONFIG event=%d", eventId);
        }
    }
}
