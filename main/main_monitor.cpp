#include "main_includes.hpp"

app::cfg::ConfigStore *cfgStore{};
app::https::HttpsServer *httpsServer{};
app::i2c::bme280::Bme280 *bme280{};
app::i2c::bus::I2cBus *i2cBus{};
app::mqtt::MqttClient *mqttClient{};
app::ota::OtaManager *otaManager{};
app::rm::ResourceManager *rscManager{};
app::sm::StateMachine *stateMachine{};
app::sntp::SntpClient *sntpClient{};
app::wm::WifiManager *wifiManager{};

template <class T>
esp_err_t checkAndInitComponent(T *&t, const char *tName)
{
    if (nullptr == t)
    {
        ESP_LOGE(TAG, "Failed to create '%s'", tName);
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t ret = t->init();
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "Failed to init '%s': %s", tName, esp_err_to_name(ret));
        finiAndDeleteComponents();
        return ret;
    }

    return ESP_OK;
}

template <class T>
esp_err_t createAndInitComponent(T *&t, const char *tName)
{
    if (nullptr != t)
        return ESP_OK;

    t = new T();
    return checkAndInitComponent(t, tName);
}

template <class T>
esp_err_t createAndInitComponentCfgSm(T *&t, const char *tName)
{
    if (nullptr != t)
        return ESP_OK;

    if (nullptr == cfgStore || !cfgStore->isInitialized() ||
        nullptr == stateMachine || !stateMachine->isInitialized())
    {
        ESP_LOGE(TAG, "Config Store or State Machine missing for '%s'", tName);
        return ESP_ERR_INVALID_STATE;
    }

    t = new T(*cfgStore, *stateMachine);
    return checkAndInitComponent(t, tName);
}

esp_err_t createAndInitI2CBus()
{
    if (nullptr != i2cBus)
        return ESP_OK;

    if (nullptr == cfgStore || !cfgStore->isInitialized())
    {
        ESP_LOGE(TAG, "Config Store missing for 'I2C Bus'");
        return ESP_ERR_INVALID_STATE;
    }

    i2cBus = new app::i2c::bus::I2cBus(app::cfg::DeviceID::I2C_BUS, *cfgStore);
    return checkAndInitComponent(i2cBus, "I2C Bus");
}

esp_err_t createAndInitBme280()
{

    if (nullptr != bme280)
        return ESP_OK;

    if (nullptr == cfgStore || !cfgStore->isInitialized() ||
        nullptr == stateMachine || !stateMachine->isInitialized() ||
        nullptr == i2cBus || !i2cBus->isInitialized())
    {
        ESP_LOGE(TAG, "Config Store or I2C Bus missing for 'BME280'");
        return ESP_ERR_INVALID_STATE;
    }

    bme280 = new app::i2c::bme280::Bme280(app::cfg::DeviceID::I2C_BME280, *cfgStore, *stateMachine, *i2cBus);
    return checkAndInitComponent(bme280, "BME280");
}

esp_err_t createAndInitComponents()
{
    logTaskStackAndHeap();

    ESP_RETURN_ON_ERROR(createAndInitComponent(rscManager, "Resource Manager"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponent(cfgStore, "Config Store"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponent(stateMachine, "State Machine"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponentCfgSm(wifiManager, "Wifi Manager"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponentCfgSm(sntpClient, "SNTP Client"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponentCfgSm(httpsServer, "HTTPS Server"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponentCfgSm(otaManager, "OTA Manager"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitComponentCfgSm(mqttClient, "MQTT Client"), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitI2CBus(), TAG, "");
    ESP_RETURN_ON_ERROR(createAndInitBme280(), TAG, "");

    return ESP_OK;
}

static void countDownForRestart(uint8_t &countDown)
{
    if (countDown > 0u)
        --countDown;
    else
        requestRestart();
}

template <class T>
void monitorComponent(T *&t)
{
    if (isRestartRequested())
        return;

    static uint8_t countDown = 60u;

    if (t->isInitialized())
        return;

    if (ESP_OK == t->reset())
    {
        countDown = 60u;
        return;
    }

    countDownForRestart(countDown);
}

void monitorComponents()
{
    logTaskStackAndHeap();

    monitorComponent(rscManager);
    monitorComponent(cfgStore);
    monitorComponent(stateMachine);
    monitorComponent(wifiManager);
    monitorComponent(sntpClient);
    monitorComponent(httpsServer);
    monitorComponent(otaManager);
    monitorComponent(mqttClient);
    monitorComponent(i2cBus);
    monitorComponent(bme280);
}

template <class T>
void finiAndDeleteComponent(T *&t)
{
    if (nullptr == t)
        return;

    t->fini();
    delete t;
    t = nullptr;
}

void finiAndDeleteComponents()
{
    finiAndDeleteComponent(bme280);
    finiAndDeleteComponent(i2cBus);
    finiAndDeleteComponent(mqttClient);
    finiAndDeleteComponent(otaManager);
    finiAndDeleteComponent(httpsServer);
    finiAndDeleteComponent(sntpClient);
    finiAndDeleteComponent(wifiManager);
    finiAndDeleteComponent(stateMachine);
    finiAndDeleteComponent(cfgStore);
    finiAndDeleteComponent(rscManager);
}
