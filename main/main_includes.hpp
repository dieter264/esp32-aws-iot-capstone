#pragma once

#include "esp_check.h"
#include "esp_event.h"
#include "app/cfg/config_store.hpp"
#include "app/cfg/firmware.hpp"
#include "app/cmn/log.hpp"
#include "app/device/device_manager.hpp"
#include "app/https/https_server.hpp"
#include "app/mqtt/mqtt_client.hpp"
#include "app/ota/ota_manager.hpp"
#include "app/rm/resource_manager.hpp"
#include "app/sm/state_machine.hpp"
#include "app/sntp/sntp_client.hpp"
#include "app/wm/wifi_manager.hpp"

const char *const TAG = "main";

enum TaskWakeBit : EventBits_t
{
    RESTART_REQUEST = 1u << 0
};

constexpr EventBits_t TASK_WAIT_BIT_MASK =
    TaskWakeBit::RESTART_REQUEST;

extern app::cfg::ConfigStore *cfgStore;
extern app::dm::DeviceManager *deviceManager;
extern app::https::HttpsServer *httpsServer;
extern app::mqtt::MqttClient *mqttClient;
extern app::ota::OtaManager *otaManager;
extern app::rm::ResourceManager *rscManager;
extern app::sm::StateMachine *stateMachine;
extern app::sntp::SntpClient *sntpClient;
extern app::wm::WifiManager *wifiManager;

esp_err_t createAndInitComponents();
void monitorComponents();
void finiAndDeleteComponents();

bool isRestartRequested();
void requestRestart();
void logTaskStackAndHeap();
