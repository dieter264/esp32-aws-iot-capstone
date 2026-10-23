#pragma once

#include <inttypes.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_types.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "app/cfg/config_store.hpp"
#include "app/cmn/log.hpp"
#include "app/sm/state_machine.hpp"
#include "app/mqtt/mqtt_client.hpp"

namespace app::mqtt
{
    const char *const TAG = "app_mqtt";

    enum TaskWakeBit : EventBits_t
    {
        STOP               = 1u << 0,
        STOP_ACK           = 1u << 1,
        CONFIG_UPDATE      = 1u << 2,
        WIFI_UPDATE        = 1u << 3,
        BME280_DATA_READY  = 1u << 4,
        DS18B20_DATA_READY = 1u << 5,
        TL136_DATA_READY   = 1u << 6
    };

    constexpr EventBits_t TASK_WAIT_BIT_MASK =
        TaskWakeBit::STOP |
        TaskWakeBit::CONFIG_UPDATE |
        TaskWakeBit::WIFI_UPDATE |
        TaskWakeBit::BME280_DATA_READY |
        TaskWakeBit::DS18B20_DATA_READY |
        TaskWakeBit::TL136_DATA_READY;

    struct MqttClient::State
    {
        explicit State(const app::cfg::ConfigStore &cs, app::sm::StateMachine &sm)
            : cfgStore_(cs), stateMachine_(sm) {}

        bool isInitialized_ = false;
        const app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;

        EventGroupHandle_t eventGroup_{};
        TaskHandle_t taskHandle_{};
        esp_event_handler_instance_t eventHandler_{};

        esp_err_t initTask();
        esp_err_t initEvents();

        void finiTask();
        void finiEvents();

        static void taskFunction(void *);
        void runTask();

        static void eventHandler(void *, esp_event_base_t, int32_t, void *);
        void handleEvent(const char *eventBase, int32_t eventId, void *eventData);

        MqttClientConfig lastUsedMqttClientConfig_{};
        esp_mqtt_client_handle_t espMqttClt_{};
        bool mqttEventHandlerRegistered_{false};
        bool retryStartPending_{false};
        bool espMqttCltStarted_{false};

        static void mqttEventHandler(void *, esp_event_base_t, int32_t, void *);
        void handleMqttEvent(int32_t eventId, void *eventData);

        void startClient();
        void stopClient();
        void createAndStartClient();
        void stopAndDestroyClient();
        void publishLatestBme280Data();
        void publishLatestDs18b20Data();
        void publishLatestTl136Data();
        void publishLatest(const char *topic, const char *payload, int payloadLen);

        bool isAppMqttConfigAvailable(const MqttClientConfig &);

        void setMqttState(MqttState);
    };
}
