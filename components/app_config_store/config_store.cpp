#include "private_include/config_internal.hpp"

namespace app::cfg
{
    constexpr auto CS_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_CS_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < CS_LOG_LEVEL)
        {
            esp_log_level_set(TAG, CS_LOG_LEVEL);
        }
    }

    ConfigStore::ConfigStore()
        : state_(std::make_unique<State>())
    {
    }

    ConfigStore::~ConfigStore()
    {
        fini();
    }

    esp_err_t ConfigStore::init()
    {
        if (isInitialized_) return ESP_OK;

        state_->initWifiMgrApConfig();
        state_->initHttpsServerConfig();
        state_->initSntpClientConfig();

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, state_->readNvs, (), "NVS read");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        isInitialized_ = true;
        ESP_LOGD(TAG, "ConfigStore initialized");
        return ESP_OK;
    }

    esp_err_t ConfigStore::update(const AppConfig &newConfig)
    {
        ESP_RETURN_ON_FALSE(isInitialized_, ESP_ERR_INVALID_STATE,
                            TAG, "ConfigStore not initialized");

        std::lock_guard updateLock(state_->mutexUpdate_);
        auto ret = state_->updateNvs(newConfig);
        if (ESP_OK == ret)
        {
            std::lock_guard cacheLock(state_->mutexCache_);
            state_->appConfig_ = newConfig;
        }

        return ret;
    }

    void ConfigStore::fini()
    {
        {
            std::lock_guard cacheLock(state_->mutexCache_);
            state_->appConfig_ = {};
        }

        if (!isInitialized_) return;
        isInitialized_ = false;
        ESP_LOGD(TAG, "ConfigStore finalized");
    }

    esp_err_t ConfigStore::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset ConfigStore");

        ESP_LOGD(TAG, "ConfigStore reset");
        return ESP_OK;
    }

    bool ConfigStore::isInitialized() const
    {
        return isInitialized_;
    }

    void ConfigStore::getSnapshot(AppConfig &appConfig) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        appConfig = state_->appConfig_;
    }

    void ConfigStore::getI2cBusConfig(app::i2c::bus::I2cBusConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.i2cBus;
    }

    void ConfigStore::getBme280Config(app::i2c::bme280::Bme280Config &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.bme280;
    }

    void ConfigStore::getWifiApConfig(app::wm::WifiManagerApConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.wifiMgrAP;
    }

    void ConfigStore::getWifiStaConfig(app::wm::WifiManagerStaConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.wifiMgrSta;
    }

    void ConfigStore::getHttpsServerConfig(app::https::HttpsServerConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.httpsSrv;
    }

    void ConfigStore::getOtaManagerConfig(app::ota::OtaManagerConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.otaMgr;
    }

    void ConfigStore::getMqttClientConfig(app::mqtt::MqttClientConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.mqttClt;
    }

    void ConfigStore::getSntpClientConfig(app::sntp::SntpClientConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.sntpClt;
    }

    void ConfigStore::State::initWifiMgrApConfig()
    {
        auto &wmApCfg = appConfig_.wifiMgrAP;

        const auto ssid_len = sizeof(CONFIG_APP_WM_AP_SSID) - 1;
        static_assert(1 <= ssid_len && app::wm::SSID_BUF_SIZE > ssid_len);
        cstrCopy(wmApCfg.ssid, CONFIG_APP_WM_AP_SSID);

        const auto passwd_len = sizeof(CONFIG_APP_WM_AP_PASSWD) - 1;
        static_assert(8 <= passwd_len && app::wm::PASS_BUF_SIZE > passwd_len);
        cstrCopy(wmApCfg.password, CONFIG_APP_WM_AP_PASSWD);

        appConfig_.wifiMgrAP.channel = CONFIG_APP_WM_AP_CHANNEL;
    }

    void ConfigStore::State::initHttpsServerConfig()
    {
        auto &httpsSrvCfg = appConfig_.httpsSrv;

        const auto user_len = sizeof(CONFIG_APP_HTTPS_LOGIN_USER) - 1;
        static_assert(1 <= user_len && app::https::USER_BUF_SIZE > user_len);
        cstrCopy(httpsSrvCfg.username, CONFIG_APP_HTTPS_LOGIN_USER);

        const auto passwd_len = sizeof(CONFIG_APP_HTTPS_LOGIN_PASSWD) - 1;
        static_assert(8 <= passwd_len && app::https::PASS_BUF_SIZE > passwd_len);
        cstrCopy(httpsSrvCfg.password, CONFIG_APP_HTTPS_LOGIN_PASSWD);
    }

    void ConfigStore::State::initSntpClientConfig()
    {
#ifdef CONFIG_APP_SNTP_ENABLED
        auto &sntpCltCfg        = appConfig_.sntpClt;
        sntpCltCfg.sntp_enabled = true;

#ifdef CONFIG_APP_SNTP_SERVER_FROM_DHCP
        constexpr bool SNTP_SERVER_FROM_DHCP = true;
        sntpCltCfg.server_from_dhcp          = true;
#else
        constexpr bool SNTP_SERVER_FROM_DHCP = false;
#endif

#ifdef CONFIG_APP_SNTP_STATIC_SERVER_1
        constexpr auto s1_len       = sizeof(CONFIG_APP_SNTP_STATIC_SERVER_1) - 1;
        constexpr bool dhcpOrStatic = (SNTP_SERVER_FROM_DHCP || 1 <= s1_len);
        static_assert(dhcpOrStatic && app::sntp::SNTP_SERVER_NAME_SIZE > s1_len);
        cstrCopy(sntpCltCfg.static_sntp_server[0], CONFIG_APP_SNTP_STATIC_SERVER_1);
#else
        static_assert(SNTP_SERVER_FROM_DHCP);
#endif

#ifdef CONFIG_APP_SNTP_STATIC_SERVER_2
        constexpr auto s2_len = sizeof(CONFIG_APP_SNTP_STATIC_SERVER_2) - 1;
        static_assert(app::sntp::SNTP_SERVER_NAME_SIZE > s2_len);
        cstrCopy(sntpCltCfg.static_sntp_server[1], CONFIG_APP_SNTP_STATIC_SERVER_2);
#endif

#endif
    }
}
