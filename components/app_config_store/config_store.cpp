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

        state_->initWifiManagerApConfig();
        state_->initHttpsServerConfig();
        state_->initSntpClientConfig();
        state_->initDeviceManagerConfig();
        state_->initBme280Config();
        state_->initOneWireBusConfig();
        state_->initDs18b20Config();
        state_->initModbusBusConfig();
        state_->initTl136Config();

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

    void ConfigStore::getDeviceManagerConfig(app::dm::DeviceManagerConfig &config) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        config = state_->appConfig_.deviceMgr;
    }

    void ConfigStore::getOneWireBusConfig(app::onewire::bus::OneWireBusConfig &cfg) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        cfg = state_->appConfig_.oneWireBus;
    }

    void ConfigStore::getDs18b20Config(app::onewire::ds18b20::Ds18b20Config &cfg) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        cfg = state_->appConfig_.ds18b20;
    }

    void ConfigStore::getModbusBusConfig(app::modbus::bus::ModbusBusConfig &cfg) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        cfg = state_->appConfig_.modbusBus;
    }

    void ConfigStore::getTl136Config(app::modbus::tl136::Tl136Config &cfg) const
    {
        configASSERT(isInitialized_);
        std::lock_guard cacheLock(state_->mutexCache_);
        cfg = state_->appConfig_.tl136;
    }
}
