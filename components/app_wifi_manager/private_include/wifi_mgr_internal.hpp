#pragma once

#include "esp_check.h"
#include "esp_wifi.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/sm/state_machine.hpp"
#include "app/wm/wifi_manager.hpp"

namespace app::wm
{
    const char *const TAG = "app_wifi";

    enum TaskWakeBit : EventBits_t
    {
        STOP             = 1u << 0,
        STOP_ACK         = 1u << 1,
        STA_GOT_IP       = 1u << 2,
        STA_DISCONNECTED = 1u << 3,
        CONFIG_UPDATED   = 1u << 4
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP |
        TaskWakeBit::STA_GOT_IP |
        TaskWakeBit::STA_DISCONNECTED |
        TaskWakeBit::CONFIG_UPDATED;

    struct WifiManager::State
    {
        explicit State(const app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm)
            : cfgStore_(store), stateMachine_(sm) {}

        bool isInitialized_ = false;
        const app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;

        TaskHandle_t taskHandle_       = nullptr;
        EventGroupHandle_t eventGroup_ = nullptr;

        esp_event_handler_instance_t ipEventHandlerInstance_     = nullptr;
        esp_event_handler_instance_t wifiEventHandlerInstance_   = nullptr;
        esp_event_handler_instance_t configEventHandlerInstance_ = nullptr;

        esp_netif_t *esp_netif_ap_  = nullptr;
        esp_netif_t *esp_netif_sta_ = nullptr;

        bool isPendingStaCfgUpdate_ = false;

        esp_err_t initTask();
        esp_err_t initEvents();
        esp_err_t initWifiApSta();

        void finiTask();
        void finiEvents();
        void finiWifiApSta();

        void handleIpEvent(const char *eventBase, int32_t eventId, void *eventData);
        void handleWifiEvent(const char *eventBase, int32_t eventId, void *eventData);
        void handleConfigEvent(const char *eventBase, int32_t eventId, void *eventData);

        static void ipEventHandler(void *, esp_event_base_t, int32_t, void *);
        static void wifiEventHandler(void *, esp_event_base_t, int32_t, void *);
        static void configEventHandler(void *, esp_event_base_t, int32_t, void *);

        static void taskFunction(void *);
        void runTask();

        esp_err_t connectWifiSta();
        void disconnectWifiSta();
        bool isWifiStaConfigUpdate();

        void getApRecord(wifi_ap_record_t &, const wifi_event_sta_connected_t &);
    };
}
