#pragma once

#include "app/modbus/tl136.hpp"

#include <type_traits>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_timer.h"

#include "r4cva02.hpp"

#include "app/cfg/config_store.hpp"
#include "app/cfg/types.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/time.hpp"
#include "app/modbus/mb_bus.hpp"
#include "app/sm/state_machine.hpp"
#include "app/stat/types.hpp"

namespace app::modbus::tl136
{
    constexpr auto TAG = "app_modbus_tl136";

    enum TaskWakeBit : EventBits_t
    {
        STOP     = 1u << 0,
        STOP_ACK = 1u << 1
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP;

    struct Tl136::State
    {
        explicit State(app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm,
                       app::modbus::bus::ModbusBus &bus)
            : cfgStore_(store),
              stateMachine_(sm),
              modebusBus_(bus) {}

        bool isInitialized_ = false;
        Tl136Config initialCfg_{};

        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        app::modbus::bus::ModbusBus &modebusBus_;
        R4cva02 *r4cva02Driver_{};

        Tl136Data tl136Data_{};

        TaskHandle_t taskHandle_{};
        EventGroupHandle_t eventGroup_{};

        esp_err_t initDevice();
        esp_err_t initTask();

        void finiDevice();
        void finiTask();

        static void taskFunction(void *);
        void runTask();

        esp_err_t compensate(uint16_t mAx100);
    };
}