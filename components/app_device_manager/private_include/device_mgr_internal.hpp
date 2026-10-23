#pragma once

#include <algorithm>
#include <bit>
#include <bitset>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_event.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/device/device_manager.hpp"
#include "app/i2c/bme280.hpp"
#include "app/i2c/i2c_bus.hpp"
#include "app/modbus/mb_bus.hpp"
#include "app/modbus/tl136.hpp"
#include "app/onewire/onewire_bus.hpp"
#include "app/onewire/ds18b20.hpp"
#include "app/sm/state_machine.hpp"

namespace app::dm
{
    constexpr auto TAG = "app_device_manager";

    enum TaskWakeBit : EventBits_t
    {
        STOP          = 1u << 0,
        STOP_ACK      = 1u << 1,
        UPDATE_DEVICE = 1u << 2,
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP |
        TaskWakeBit::UPDATE_DEVICE;

    struct DeviceManager::State
    {
        explicit State(app::cfg::ConfigStore &store,
                       app::sm::StateMachine &sm)
            : cfgStore_(store),
              stateMachine_(sm),
              runtime_(store, sm) {}

        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        bool isInitialized_ = false;

        TaskHandle_t taskHandle_{};
        EventGroupHandle_t eventGroup_{};
        esp_event_handler_instance_t deviceEvtHdlr_{};

        esp_err_t initTask();
        esp_err_t initEvents();

        void finiTask();
        void finiEvents();

        static void taskFunction(void *);
        void runTask();

        static void deviceEventHandler(void *, esp_event_base_t, int32_t, void *);
        void handleDeviceEvent(const char *eventBase, int32_t eventId, void *eventData);

        struct I2cRuntime
        {
            using BusType = app::i2c::bus::I2cBus;
            BusType *bus{};
            static constexpr auto busName     = "I2C Bus";
            static constexpr uint32_t devMask = I2C_DEVICE_MASK;
        };

        struct Bme280Runtime
        {
            using DeviceType = app::i2c::bme280::Bme280;
            DeviceType *device{};
            uint32_t retryIterMillis{};
            TickType_t retryStartTicks{};
            static constexpr DeviceID deviceID = DeviceID::I2C_BME280;
            static constexpr auto deviceName   = "BME280";
        };

        struct OneWireRuntime
        {
            using BusType = app::onewire::bus::OneWireBus;
            BusType *bus{};
            static constexpr auto busName     = "1-Wire Bus";
            static constexpr uint32_t devMask = ONEWIRE_DEVICE_MASK;
        };

        struct Ds18b20Runtime
        {
            using DeviceType = app::onewire::ds18b20::Ds18b20;
            DeviceType *device{};
            uint32_t retryIterMillis{};
            TickType_t retryStartTicks{};
            static constexpr DeviceID deviceID = DeviceID::ONEWIRE_DS18B20;
            static constexpr auto deviceName   = "DS18B20";
        };

        struct ModbusRuntime
        {
            using BusType = app::modbus::bus::ModbusBus;
            BusType *bus{};
            static constexpr auto busName     = "Modbus Bus";
            static constexpr uint32_t devMask = MODBUS_DEVICE_MASK;
        };

        struct Tl136Runtime
        {
            using DeviceType = app::modbus::tl136::Tl136;
            DeviceType *device{};
            uint32_t retryIterMillis{};
            TickType_t retryStartTicks{};
            static constexpr DeviceID deviceID = DeviceID::MODBUS_TL136;
            static constexpr auto deviceName   = "TL136";
        };

        struct Runtime
        {
            explicit Runtime(app::cfg::ConfigStore &store,
                             app::sm::StateMachine &sm)
                : cfgStore_(store),
                  stateMachine_(sm) {}

            app::cfg::ConfigStore &cfgStore_;
            app::sm::StateMachine &stateMachine_;

            DeviceManagerConfig configured_{};
            FlagField active_{};
            DeviceManagerConfig lastConfigured_{};
            FlagField lastActive_{};

            I2cRuntime i2c_{};
            Bme280Runtime bme280_{};

            OneWireRuntime onewire_{};
            Ds18b20Runtime ds18b20_{};

            ModbusRuntime modbus_{};
            Tl136Runtime tl136_{};

            bool isFirstDeviceLog_{true};

            template <class Tbus>
            esp_err_t checkBusAvailable(Tbus &);

            void activateDevices();
            template <class Tbus, class Tdev>
            void activateDevice(Tbus &, Tdev &);
            template <class Tbus>
            esp_err_t activateBus(Tbus &);

            void monitorDevices();
            template <class Tbus, class Tdev>
            void monitorDevice(Tbus &, Tdev &);

            void deactivateDevices(uint32_t = ALL_DEVICE_MASK);
            template <class Tdev>
            void deactivateDevice(Tdev &, uint32_t devMask);
            template <class Tbus>
            void deactivateBus(Tbus &, uint32_t devMask);

            void updateStateMachine();

            bool statefulLogDone_{false};
            uint8_t currentMbDevAddr_{1u};
            std::bitset<256> foundMbDevAddr_{};

            void infoLogDevices() const;
            void statefulLogConnectedDevices();

            uint32_t getTaskWaitToutMs();

            void reset();
        };

        Runtime runtime_;
    };
}
