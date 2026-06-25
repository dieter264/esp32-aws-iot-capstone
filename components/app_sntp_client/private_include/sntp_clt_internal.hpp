#pragma once

#include <atomic>
#include <inttypes.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_sntp.h"
#include "esp_netif_types.h"
#include "esp_timer.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/time.hpp"
#include "app/sm/state_machine.hpp"
#include "app/sntp/sntp_client.hpp"

namespace app::sntp
{
    const char *const TAG = "app_sntp";

    enum TaskWakeBit : EventBits_t
    {
        STOP          = 1u << 0,
        STOP_ACK      = 1u << 1,
        CONFIG_UPDATE = 1u << 2,
        WIFI_UPDATE   = 1u << 3,
        SNTP_SYNC     = 1u << 4
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP |
        TaskWakeBit::CONFIG_UPDATE |
        TaskWakeBit::WIFI_UPDATE |
        TaskWakeBit::SNTP_SYNC;

    struct SntpClient::State
    {
        explicit State(const app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
            : cfgStore_(cs), stateMachine_(sm) {}

        bool isInitialized_ = false;
        const app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;

        EventGroupHandle_t eventGroup_{};
        TaskHandle_t taskHandle_{};
        esp_event_handler_instance_t appEventHandler_{};
        esp_event_handler_instance_t sntpEventHandler_{};

        esp_err_t initTask();
        esp_err_t initEvents();

        void finiTask();
        void finiEvents();

        static void taskFunction(void *);
        void runTask();

        static void eventHandler(void *, esp_event_base_t, int32_t, void *);
        void handleEvent(const char *eventBase, int32_t eventId, void *eventData);

        std::atomic<uint64_t> lastSyncEventUtcUs_{0};
        SntpClientConfig lastUsedSntpClientConfig_{};
        bool espSntpCltInitialized_{false};
        bool retryStartPending_{false};
        bool espSntpCltStarted_{false};

        void startClient();
        void createAndStartClient();
        void destroyClient();

        bool isAppSntpConfigAvailable(const SntpClientConfig &);
    };
}
