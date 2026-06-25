#include "private_include/wifi_mgr_internal.hpp"

namespace app::wm
{
    constexpr auto WM_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_WM_LOG_LEVEL);

    static_assert(sizeof(((wifi_ap_config_t *)0)->ssid) == (app::wm::SSID_BUF_SIZE - 1));
    static_assert(sizeof(((wifi_ap_config_t *)0)->password) == app::wm::PASS_BUF_SIZE);

    void apply_log_level()
    {
        if (-1 < WM_LOG_LEVEL)
        {
            esp_log_level_set(TAG, WM_LOG_LEVEL);
        }
    }

    WifiManager::WifiManager(const app::cfg::ConfigStore &store, app::sm::StateMachine &sm)
        : state_(std::make_unique<State>(store, sm)) {}

    WifiManager::~WifiManager()
    {
        fini();
    }

    esp_err_t WifiManager::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, state_->initTask, (), "Task initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initEvents, (), "Event handling initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initWifiApSta, (), "Wifi initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        ESP_LOGD(TAG, "Wifi Manager initialized");
        state_->isInitialized_ = true;
        return ESP_OK;
    }

    void WifiManager::fini()
    {
        state_->finiWifiApSta();
        state_->finiEvents();
        state_->finiTask();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "Wifi Manager finalized");
    }

    esp_err_t WifiManager::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset Wifi Manager");

        ESP_LOGD(TAG, "Wifi Manager reset");
        return ESP_OK;
    }

    bool WifiManager::isInitialized() const
    {
        return state_->isInitialized_;
    }

    const app::cfg::ConfigStore &WifiManager::getConfigStore() const
    {
        return state_->cfgStore_;
    }

    app::sm::StateMachine &WifiManager::getStateMachine()
    {
        return state_->stateMachine_;
    }

    esp_err_t WifiManager::State::initWifiApSta()
    {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg),
                            TAG, "Could not allocate Wifi resources");

        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA),
                            TAG, "Could not set Wifi mode APSTA");

        esp_netif_ap_ = esp_netif_create_default_wifi_ap();

        app::wm::WifiManagerApConfig apCfg;
        cfgStore_.getWifiApConfig(apCfg);

        wifi_config_t ap_cfg{};
        ap_cfg.ap.ssid_len = app::cfg::bufCopy(ap_cfg.ap.ssid, apCfg.ssid);
        app::cfg::cstrCopy(ap_cfg.ap.password, apCfg.password);
        ap_cfg.ap.channel          = apCfg.channel;
        ap_cfg.ap.max_connection   = 1;
        ap_cfg.ap.authmode         = WIFI_AUTH_WPA2_PSK;
        ap_cfg.ap.pmf_cfg.required = false;

        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg),
                            TAG, "Could not set AP config");

        esp_netif_ip_info_t ipInfo;
        esp_netif_get_ip_info(esp_netif_ap_, &ipInfo);
        ESP_LOGD(TAG, "Wifi AP configured: IP=" IPSTR ", SSID=%s, channel=%d",
                 IP2STR(&ipInfo.ip), ap_cfg.ap.ssid, ap_cfg.ap.channel);

        esp_netif_sta_ = esp_netif_create_default_wifi_sta();

        ESP_RETURN_ON_ERROR(esp_wifi_start(),
                            TAG, "Could not start AP and STA ctrl blocks");

        return ESP_OK;
    }

    void WifiManager::State::finiWifiApSta()
    {
        esp_wifi_stop();
        esp_wifi_deinit();

        if (nullptr != esp_netif_sta_)
        {
            esp_netif_destroy_default_wifi(esp_netif_sta_);
            esp_netif_sta_ = nullptr;
        }

        if (nullptr != esp_netif_ap_)
        {
            esp_netif_destroy_default_wifi(esp_netif_ap_);
            esp_netif_ap_ = nullptr;
        }
    }

    esp_err_t WifiManager::State::connectWifiSta()
    {
        app::wm::WifiManagerStaConfig staCfg;
        cfgStore_.getWifiStaConfig(staCfg);
        if (('\0' == staCfg.ssid[0]) || ('\0' == staCfg.password[0]))
        {
            ESP_LOGE(TAG, "No SSID or password available for STA mode");
            isPendingStaCfgUpdate_ = false;
            return ESP_ERR_INVALID_ARG;
        }

        if (isPendingStaCfgUpdate_)
        {
            wifi_config_t sta_cfg{};
            app::cfg::bufCopy(sta_cfg.sta.ssid, staCfg.ssid);
            app::cfg::bufCopy(sta_cfg.sta.password, staCfg.password);

            sta_cfg.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
            sta_cfg.sta.sort_method        = WIFI_CONNECT_AP_BY_SIGNAL;
            sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            sta_cfg.sta.sae_pwe_h2e        = WPA3_SAE_PWE_BOTH;
            sta_cfg.sta.failure_retry_cnt  = 3;

            ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg),
                                TAG, "Could not set STA config");

            isPendingStaCfgUpdate_ = false;
        }

        LOGD_CALL_RET(TAG, esp_wifi_connect, (),
                      "Wifi connect to SSID=%s", staCfg.ssid);
    }

    void WifiManager::State::disconnectWifiSta()
    {
        esp_wifi_disconnect();
    }

    bool WifiManager::State::isWifiStaConfigUpdate()
    {
        app::wm::WifiManagerStaConfig cachedStaCfg;
        cfgStore_.getWifiStaConfig(cachedStaCfg);

        wifi_config_t sta_cfg{};
        const auto ret = esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);
        if (ESP_OK != ret)
        {
            ESP_LOGD(TAG, "Could not get current wifi config: %s", esp_err_to_name(ret));
            return false;
        }

        app::wm::WifiManagerStaConfig activeStaCfg{};
        app::cfg::cstrCopy(activeStaCfg.ssid, reinterpret_cast<const char *>(sta_cfg.sta.ssid));
        app::cfg::cstrCopy(activeStaCfg.password, reinterpret_cast<const char *>(sta_cfg.sta.password));

        return (cachedStaCfg != activeStaCfg);
    }

    void WifiManager::State::getApRecord(wifi_ap_record_t &rec, const wifi_event_sta_connected_t &event)
    {
        rec = {};
        if (ESP_OK == esp_wifi_sta_get_ap_info(&rec)) return;

        memcpy(rec.ssid, event.ssid, event.ssid_len);
        rec.ssid[event.ssid_len] = '\0';

        int rssi;
        if (ESP_OK == esp_wifi_sta_get_rssi(&rssi))
        {
            rec.rssi = static_cast<int8_t>(rssi);
        }
        else
        {
            rec.rssi = RSSI_INVALID_DBM;
        }
    }
}
