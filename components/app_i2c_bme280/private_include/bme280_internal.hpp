#pragma once

#include <type_traits>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "app/cfg/types.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/time.hpp"
#include "app/cfg/config_store.hpp"
#include "app/i2c/i2c_bus.hpp"
#include "app/i2c/bme280.hpp"
#include "app/sm/state_machine.hpp"
#include "app/stat/types.hpp"

namespace app::i2c::bme280
{
    constexpr auto TAG = "app_i2c_bme280";

    enum TaskWakeBit : EventBits_t
    {
        STOP     = 1u << 0,
        STOP_ACK = 1u << 1
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP;

    struct Bme280Calib
    {
        int32_t T1{}, T2{}, T3{};
        int64_t P1{}, P2{}, P3{}, P4{}, P5{}, P6{}, P7{}, P8{}, P9{};
        int32_t H1{}, H2{}, H3{}, H4{}, H5{}, H6{};
    };

    struct Bme280::State
    {
        explicit State(app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm,
                       app::i2c::bus::I2cBus &bus)
            : cfgStore_(store), stateMachine_(sm), i2cBus_(bus) {}

        bool isInitialized_ = false;
        Bme280Config initialCfg_{};

        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        app::i2c::bus::I2cBus &i2cBus_;

        Bme280Data bme280Data_;
        Bme280Calib calib_;
        i2c_master_dev_handle_t devHandle_ = nullptr;

        TaskHandle_t taskHandle_       = nullptr;
        EventGroupHandle_t eventGroup_ = nullptr;

        esp_err_t initDevice();
        esp_err_t initCalibration();
        esp_err_t initConfig();
        esp_err_t initTask();

        void finiDevice();
        void finiTask();

        static void taskFunction(void *);
        void runTask();

        esp_err_t burstReadAdc(int32_t &adcT, int32_t &adcP, int32_t &adcH);
        void compensate(const int32_t &adcT, const int32_t &adcP, const int32_t &adcH);
    };
}