#pragma once

#include <type_traits>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "app/cfg/types.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/time.hpp"
#include "app/cfg/config_store.hpp"
#include "app/onewire/ds18b20.hpp"
#include "app/onewire/onewire_bus.hpp"
#include "app/sm/state_machine.hpp"
#include "app/stat/types.hpp"

namespace app::onewire::ds18b20
{
    constexpr auto TAG = "app_1wire_ds18b20";

    enum TaskWakeBit : EventBits_t
    {
        STOP     = 1u << 0,
        STOP_ACK = 1u << 1
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP;

    constexpr uint8_t ONEWIRE_CMD_DS18B20_CONVERT_T        = 0x44;
    constexpr uint8_t ONEWIRE_CMD_DS18B20_READ_SCRATCHPAD  = 0xBE;
    constexpr uint8_t ONEWIRE_CMD_DS18B20_WRITE_SCRATCHPAD = 0x4E;

    constexpr size_t DS18B20_READ_SCRATCHPAD_SIZE  = 9u;
    constexpr size_t DS18B20_WRITE_SCRATCHPAD_SIZE = 3u;
    constexpr uint8_t DS18B20_FAMILY_CODE          = 0x28;
    constexpr uint8_t DS18B20_12BIT_CONF           = 0x60;
    constexpr float TEMP_INC_12BIT_X100            = 100.0f / 16.0f;

    struct Ds18b20::State
    {
        explicit State(app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm,
                       app::onewire::bus::OneWireBus &bus)
            : cfgStore_(store), stateMachine_(sm), onewireBus_(bus) {}

        bool isInitialized_ = false;
        Ds18b20Config initialCfg_{};

        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        app::onewire::bus::OneWireBus &onewireBus_;

        Ds18b20Data ds18b20Data_{};

        TaskHandle_t taskHandle_{};
        EventGroupHandle_t eventGroup_{};

        esp_err_t initDevice();
        esp_err_t initTask();

        void finiDevice();
        void finiTask();

        static void taskFunction(void *);
        void runTask();

        esp_err_t readRawData(uint8_t *buf, size_t);
        esp_err_t compensate(const uint8_t *buf, size_t);
    };
}