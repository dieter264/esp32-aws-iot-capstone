#pragma once

#include "esp_check.h"
#include "esp_https_ota.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/sm/state_machine.hpp"
#include "app/ota/ota_manager.hpp"

namespace app::ota
{
    const char *const TAG = "app_ota";

    enum TaskWakeBit : EventBits_t
    {
        STOP      = 1u << 0,
        STOP_ACK  = 1u << 1,
        START_OTA = 1u << 2
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP |
        TaskWakeBit::START_OTA;

    struct OtaManager::State
    {
        explicit State(const app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm)
            : cfgStore_(store), stateMachine_(sm) {}

        bool isInitialized_ = false;
        const app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;

        TaskHandle_t taskHandle_                 = nullptr;
        EventGroupHandle_t eventGroup_           = nullptr;
        esp_event_handler_instance_t otaEvtHdlr_ = nullptr;

        esp_err_t initTask();
        esp_err_t initEvents();

        void finiTask();
        void finiEvents();
        
        static void taskFunction(void *);
        void runTask();

        static void otaEventHandler(void *, esp_event_base_t, int32_t, void *);
        void handleOtaEvent(const char *eventBase, int32_t eventId, void *eventData);
        void startOta();
        void setOtaStatus(OtaState otaState, esp_err_t error_status = ESP_OK);
    };
}
