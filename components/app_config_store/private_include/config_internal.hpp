#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_crc.h"
#include "app/cmn/log.hpp"
#include "app/cmn/mutex.hpp"
#include "app/cfg/config_store.hpp"

namespace app::cfg
{
    constexpr auto TAG                      = "app_config_store";
    constexpr auto NVS_NAMESPACE            = "app_cfg_nvs_sto";
    constexpr auto NVS_KEY                  = "app_cfg_data";
    constexpr uint32_t PERSISTENT_CFG_MAGIC = 0x43464731U;

    struct PersistentCfgHdr
    {
        uint32_t magic{};
        uint16_t version{};
        uint16_t totalSize{};
        uint32_t crc32{};
    };

    struct PersistentOtaManagerConfig
    {
        char url[app::ota::URL_SIZE]{};
        char server_cert[app::ota::SERVER_CERT_SIZE]{};
    };

    struct PersistentMqttClientConfig
    {
        char uri[app::mqtt::URI_SIZE]{};
        char broker_cert[app::mqtt::BROKER_CERT_SIZE]{};
    };

    struct PersistentCfgV1
    {
        static constexpr uint16_t VERSION = 1U;
        PersistentCfgHdr hdr{};

        app::i2c::bme280::Bme280Config bme280{};
        app::wm::WifiManagerStaConfig wifiMgrSta{};
        PersistentOtaManagerConfig otaMgr{};
        PersistentMqttClientConfig mqttClt{};
        app::sntp::SntpClientConfig sntpClt{};
        app::dm::DeviceManagerConfig deviceMgr{};
        app::onewire::ds18b20::Ds18b20Config ds18b20{};
        app::modbus::tl136::Tl136Config tl136{};
    };

    struct NvsHandleGuard
    {
        nvs_handle_t handle{};
        ~NvsHandleGuard()
        {
            if (handle)
            {
                nvs_close(handle);
                ESP_LOGD(TAG, "NVS namespace '%s' closed", NVS_NAMESPACE);
            }
        }
    };

    struct ConfigStore::State
    {
        AppConfig appConfig_{};

        app::cmn::Mutex mutexUpdate_;
        mutable app::cmn::Mutex mutexCache_;

        void initWifiManagerApConfig();
        void initHttpsServerConfig();
        void initSntpClientConfig();
        void initDeviceManagerConfig();
        void initBme280Config();
        void initOneWireBusConfig();
        void initDs18b20Config();
        void initModbusBusConfig();
        void initTl136Config();

        esp_err_t readNvs();
        esp_err_t updateNvs(const AppConfig &);

        esp_err_t eraseKey(NvsHandleGuard &);
        esp_err_t loadAndValidate(NvsHandleGuard &, PersistentCfgV1 &);
        uint32_t getCrc32(const PersistentCfgV1 &);
    };
}
