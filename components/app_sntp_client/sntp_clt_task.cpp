#include "private_include/sntp_clt_internal.hpp"

namespace app::sntp
{
    esp_err_t SntpClient::State::initTask()
    {
        if (nullptr != taskHandle_)
        {
            ESP_LOGE(TAG, "Cannot start task: previous task is still running");
            return ESP_ERR_INVALID_STATE;
        }

        eventGroup_ = xEventGroupCreate();

        ESP_RETURN_ON_FALSE(nullptr != eventGroup_, ESP_ERR_NO_MEM,
                            TAG, "Could not create event group");

        BaseType_t ret = xTaskCreate(&SntpClient::State::taskFunction,
                                     "sntp_clt_task",
                                     4096,
                                     this,
                                     configMAX_PRIORITIES - 1,
                                     &taskHandle_);

        ESP_RETURN_ON_FALSE(pdPASS == ret, ESP_ERR_NO_MEM,
                            TAG, "Could not create task");

        return ESP_OK;
    }

    void SntpClient::State::finiTask()
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

    void SntpClient::State::taskFunction(void *arg)
    {
        static_cast<SntpClient::State *>(arg)->runTask();
    }

    void SntpClient::State::runTask()
    {
        constexpr uint32_t TASK_WAIT_TIMEOUT_MS = 5000UL;

        constexpr int64_t STALE_SYNC_TIMEOUT_US = 2000LL * CONFIG_LWIP_SNTP_UPDATE_DELAY;
        int64_t lastSyncEventUptimeUs           = 0LL;

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
                app::sntp::SntpClientConfig sntpCfg{};
                cfgStore_.getSntpClientConfig(sntpCfg);
                if (lastUsedSntpClientConfig_ != sntpCfg)
                {
                    ESP_LOGI(TAG, "SNTP Client configuration changed");
                    // ESP-NETIF consumes the server set during initialization.
                    destroyClient();
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
                    ESP_LOGD(TAG, "Wi-Fi has no IP; synchronization pending");
                    retryStartPending_ = false;
                }
            }

            if (0 != (bits & TaskWakeBit::SNTP_SYNC))
            {
                const uint64_t syncUtcUs =
                    lastSyncEventUtcUs_.load(std::memory_order_acquire);

                const time_t sec = syncUtcUs / 1000000ULL;
                struct tm tmUtc{};
                gmtime_r(&sec, &tmUtc);
                char ts[32]{};
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmUtc);

                lastSyncEventUptimeUs = esp_timer_get_time();
                ESP_LOGI(TAG, "SNTP event: time synced (UTC): %s.%06ld",
                         ts, static_cast<long>(syncUtcUs % 1000000ULL));

                SntpClientStatus status = {
                    .state            = SntpState::SYNCED,
                    .is_time_valid    = true,
                    .last_sync_utc_us = syncUtcUs,
                    .last_error       = ESP_OK,
                };

                stateMachine_.updateSntpStatus(
                    [status](auto &cltStatus)
                    {
                        cltStatus = status;
                    });
            }
            else
            {
                const auto nowUs = esp_timer_get_time();

                // Before the first sync there is no healthy cadence to become stale;
                // initial acquisition and start retries are handled separately.
                if ((0LL < lastSyncEventUptimeUs) &&
                    (STALE_SYNC_TIMEOUT_US < (nowUs - lastSyncEventUptimeUs)))
                {
                    lastSyncEventUptimeUs = nowUs;
                    ESP_LOGW(TAG, "SNTP sync is stale; restarting SNTP");
                    destroyClient();
                    createAndStartClient();
                }
            }
        }

        destroyClient();

        xEventGroupSetBits(eventGroup_, TaskWakeBit::STOP_ACK);
        vTaskDelete(nullptr);
    }

    esp_err_t SntpClient::State::initEvents()
    {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, app::sm::APP_EVENT_CONFIG_UPDATE,
                                &(eventHandler), (void *)this, &(appEventHandler_)),
                            TAG, "Could not register event handler for APP ANY");

        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                app::sm::APP_EVENT, app::sm::APP_EVENT_WIFI_UPDATE,
                                &(eventHandler), (void *)this, &(appEventHandler_)),
                            TAG, "Could not register event handler for APP ANY");

        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                                NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC,
                                &(eventHandler), (void *)this, &(sntpEventHandler_)),
                            TAG, "Could not register event handler for NETIF SNTP");

        return ESP_OK;
    }

    void SntpClient::State::finiEvents()
    {
        if (nullptr != sntpEventHandler_)
        {
            esp_event_handler_instance_unregister(
                NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC,
                sntpEventHandler_);

            sntpEventHandler_ = nullptr;
        }

        if (nullptr != appEventHandler_)
        {
            esp_event_handler_instance_unregister(
                app::sm::APP_EVENT, app::sm::APP_EVENT_WIFI_UPDATE,
                appEventHandler_);

            esp_event_handler_instance_unregister(
                app::sm::APP_EVENT, app::sm::APP_EVENT_CONFIG_UPDATE,
                appEventHandler_);

            appEventHandler_ = nullptr;
        }
    }

    void SntpClient::State::eventHandler(void *arg, esp_event_base_t eventBase,
                                         int32_t eventId, void *eventData)
    {
        auto *sntpClientState = static_cast<SntpClient::State *>(arg);
        sntpClientState->handleEvent(eventBase, eventId, eventData);
    }

    void SntpClient::State::handleEvent(const char *eventBase, int32_t eventId,
                                        void *eventData)
    {
        ESP_LOGD(TAG, "Received event base=%s, id=%d", eventBase, eventId);

        if (app::sm::APP_EVENT == eventBase)
        {
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

            default:
                ESP_LOGE(TAG, "Unhandled APP event=%d", eventId);
            }
        }
        else if (NETIF_SNTP_EVENT == eventBase)
        {
            switch (eventId)
            {
            case NETIF_SNTP_TIME_SYNC:
            {
                const esp_netif_sntp_time_sync_t *syncTime =
                    static_cast<const esp_netif_sntp_time_sync_t *>(eventData);

                if (syncTime)
                {
                    const uint64_t syncUtcUs =
                        static_cast<uint64_t>(syncTime->tv.tv_sec) * 1000000ULL +
                        syncTime->tv.tv_usec;

                    lastSyncEventUtcUs_.store(syncUtcUs, std::memory_order_release);

                    xEventGroupSetBits(eventGroup_, TaskWakeBit::SNTP_SYNC);
                }
                else
                {
                    ESP_LOGW(TAG, "SNTP event: time synced (no timeval provided)");
                }
            }
            break;

            default:
                ESP_LOGE(TAG, "Unhandled SNTP event=%d", eventId);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Unhandled event base=%s", eventBase);
        }
    }
}
