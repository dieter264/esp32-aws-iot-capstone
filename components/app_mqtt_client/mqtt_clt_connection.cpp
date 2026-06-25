#include "private_include/mqtt_clt_internal.hpp"

namespace app::mqtt
{
    void MqttClient::State::mqttEventHandler(void *arg, esp_event_base_t,
                                             int32_t eventId, void *eventData)
    {
        auto *mqttClientState = static_cast<MqttClient::State *>(arg);
        mqttClientState->handleMqttEvent(eventId, eventData);
    }

    void MqttClient::State::handleMqttEvent(int32_t eventId, void *eventData)
    {
        ESP_LOGD(TAG, "Received MQTT event id=%d", eventId);

        switch (eventId)
        {
        case MQTT_EVENT_CONNECTED:
        {
            setMqttState(app::mqtt::MqttState::CONNECTED);
        }
        break;

        case MQTT_EVENT_DISCONNECTED:
        {
            setMqttState(app::mqtt::MqttState::DISCONNECTED);
        }
        break;

        case MQTT_EVENT_ERROR:
        {
            setMqttState(app::mqtt::MqttState::ERROR);
        }
        break;

        case MQTT_EVENT_PUBLISHED:
        {
            setMqttState(app::mqtt::MqttState::PUBLISHED);
        }
        break;

        default:
            ESP_LOGD(TAG, "Unhandled MQTT event=%d", eventId);
        }
    }

    void MqttClient::State::startClient()
    {
        if (!mqttEventHandlerRegistered_ || espMqttCltStarted_) return;

        auto ret = esp_mqtt_client_start(espMqttClt_);
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
            setMqttState(app::mqtt::MqttState::ERROR);
            retryStartPending_ = true;
            return;
        }

        retryStartPending_ = false;
        espMqttCltStarted_ = true;
        ESP_LOGD(TAG, "MQTT client started");
        setMqttState(app::mqtt::MqttState::CONNECTING);
    }

    void MqttClient::State::stopClient()
    {
        if (!espMqttCltStarted_ || nullptr == espMqttClt_) return;

        const auto ret = esp_mqtt_client_stop(espMqttClt_);
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        }

        espMqttCltStarted_ = false;
        ESP_LOGD(TAG, "MQTT client stopped");
        setMqttState(app::mqtt::MqttState::STOPPED);
    }

    void MqttClient::State::createAndStartClient()
    {
        if (espMqttCltStarted_) return;

        cfgStore_.getMqttClientConfig(lastUsedMqttClientConfig_);
        if (!isAppMqttConfigAvailable(lastUsedMqttClientConfig_))
        {
            app::mqtt::MqttClientStatus mqttStatus{};
            stateMachine_.getMqttStatus(mqttStatus);
            if (app::mqtt::MqttState::NOT_CONFIGURED != mqttStatus.state)
            {
                ESP_LOGI(TAG, "MQTT client not configured");
                setMqttState(app::mqtt::MqttState::NOT_CONFIGURED);
            }

            return;
        }

        esp_mqtt_client_config_t mqttCfg{};
        mqttCfg.broker.address.uri              = lastUsedMqttClientConfig_.uri;
        mqttCfg.broker.verification.certificate = lastUsedMqttClientConfig_.broker_cert.c_str();
        mqttCfg.network.disable_auto_reconnect  = false;
        mqttCfg.network.reconnect_timeout_ms    = 10000;

        espMqttClt_ = esp_mqtt_client_init(&mqttCfg);
        if (nullptr == espMqttClt_)
        {
            ESP_LOGE(TAG, "Failed to initialize MQTT client");
            setMqttState(app::mqtt::MqttState::ERROR);
            return;
        }

        auto ret = esp_mqtt_client_register_event(espMqttClt_, MQTT_EVENT_ANY, mqttEventHandler, this);
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
            stopAndDestroyClient();
            setMqttState(app::mqtt::MqttState::ERROR);
            return;
        }

        ESP_LOGD(TAG, "MQTT client initialized");
        mqttEventHandlerRegistered_ = true;

        app::wm::WifiStatus wifiStatus{};
        stateMachine_.getWifiStatus(wifiStatus);
        if (wifiStatus.sta_has_ip)
        {
            startClient();
        }
    }

    void MqttClient::State::stopAndDestroyClient()
    {
        if (nullptr == espMqttClt_) return;

        stopClient();

        if (mqttEventHandlerRegistered_)
        {
            auto ret = esp_mqtt_client_unregister_event(espMqttClt_, MQTT_EVENT_ANY, mqttEventHandler);
            if (ESP_OK != ret)
            {
                ESP_LOGE(TAG, "Failed to unregister MQTT event handler: %s", esp_err_to_name(ret));
            }
        }

        auto ret = esp_mqtt_client_destroy(espMqttClt_);
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to destroy MQTT client");
        }

        mqttEventHandlerRegistered_ = false;
        retryStartPending_          = false;
        lastUsedMqttClientConfig_   = {};
        espMqttClt_                 = nullptr;
        ESP_LOGD(TAG, "MQTT client destroyed");
    }

    void MqttClient::State::publishLatestBme280Data()
    {
        app::mqtt::MqttClientStatus mqttStatus{};
        stateMachine_.getMqttStatus(mqttStatus);
        if (app::mqtt::MqttState::CONNECTED != mqttStatus.state)
        {
            return;
        }

        app::i2c::bme280::Bme280Status bme280{};
        stateMachine_.getBme280Status(bme280);
        // DATA_READY already establishes sensor validity. UTC validity is encoded
        // separately in the payload and must not suppress an otherwise valid sample.
        static constexpr char BME280_JSON_FMT[] =
            "{"
            "\"data\":{"
            "\"temp_degCx100\":%" PRId32 ","
            "\"press_PaQ24d8\":%" PRIu32 ","
            "\"hum_RhQ22d10\":%" PRIu32
            "},"
            "\"sample_time_valid\":%s" ","
            "\"sample_time_utc_us\":%" PRIu64
            "}";

        const int payloadLen = snprintf(nullptr, 0,
                                        BME280_JSON_FMT,
                                        bme280.data.temp_degCx100,
                                        bme280.data.press_PaQ24d8,
                                        bme280.data.hum_RhQ22d10,
                                        bme280.sample_time_valid ? "true" : "false",
                                        bme280.sample_time_utc_us);
        if (0 > payloadLen)
        {
            ESP_LOGE(TAG, "BME280 JSON formatter failed");
            return;
        }

        auto payload = std::make_unique<char[]>((size_t)payloadLen + 1U);

        const int writtenLen = snprintf(payload.get(), (size_t)payloadLen + 1U,
                                        BME280_JSON_FMT,
                                        bme280.data.temp_degCx100,
                                        bme280.data.press_PaQ24d8,
                                        bme280.data.hum_RhQ22d10,
                                        bme280.sample_time_valid ? "true" : "false",
                                        bme280.sample_time_utc_us);

        if (writtenLen != payloadLen)
        {
            ESP_LOGE(TAG, "BME280 JSON buffer failed");
            return;
        }

        ESP_LOGI(TAG, "BME280 MQTT payload: %s", payload.get());

        static constexpr char TOPIC[] = "devices/001-esp32s3/01-bme280";
        static constexpr int QOS0     = 0;
        static constexpr int RETAIN0  = 0;

        auto ret = esp_mqtt_client_publish(espMqttClt_,
                                           TOPIC,
                                           payload.get(),
                                           payloadLen,
                                           QOS0,
                                           RETAIN0);
        if (0 > ret)
        {
            ESP_LOGW(TAG, "Failed to publish BME280 data");
            return;
        }

        setMqttState(app::mqtt::MqttState::PUBLISHED);
    }

    bool MqttClient::State::isAppMqttConfigAvailable(const MqttClientConfig &cfg)
    {
        return ('\0' != cfg.uri[0]) && (!cfg.broker_cert.empty());
    }

    void MqttClient::State::setMqttState(MqttState state)
    {
        stateMachine_.updateMqttStatus(
            [state](auto &mqttCltStatus)
            {
                if (app::mqtt::MqttState::PUBLISHED == state)
                {
                    mqttCltStatus.last_publish_us = esp_timer_get_time();
                }
                else
                {
                    mqttCltStatus.state = state;
                }
            });
    }
}
