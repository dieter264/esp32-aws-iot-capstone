#include "https_srv_internal.hpp"

namespace app::https
{
    extern const unsigned char CONFIG_HTML_START[] asm("_binary_config_html_start");
    extern const unsigned char CONFIG_HTML_END[] asm("_binary_config_html_end");

    esp_err_t uriHandlerGetConfig(httpd_req_t *req)
    {
        if (nullptr == req)
        {
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGD(TAG, "GET /config received, content_len=%zu", req->content_len);
        ESP_RETURN_ON_ERROR(checkBasicAuth(req), TAG, "Authorization failed");

        httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
        const char *configHtml     = reinterpret_cast<const char *>(CONFIG_HTML_START);
        const size_t configHtmlLen = CONFIG_HTML_END - CONFIG_HTML_START;
        return httpd_resp_send(req, configHtml, configHtmlLen);
    }

    esp_err_t uriHandlerPostConfig(httpd_req_t *req)
    {
        if (nullptr == req)
        {
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGD(TAG, "POST /config received, content_len=%zu", req->content_len);
        ESP_RETURN_ON_ERROR(checkBasicAuth(req), TAG, "Authorization failed");

        if (req->content_len > 8192)
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Invalid content length");
        }

        auto body = std::make_unique<char[]>(req->content_len + 1U);
        if (ESP_OK != readRequestBody(req, body.get()))
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Body read failed");
        }

        ConfigFormData form;
        if (!parseUrlEncodedForm(body.get(), form))
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Form parse failed");
        }

        auto *server = static_cast<HttpsServer *>(req->user_ctx);
        app::cfg::AppConfig appCfg;
        server->getConfigStore().getSnapshot(appCfg);

        applyFormToConfig(form, appCfg);

        if (ESP_OK != server->getConfigStore().update(appCfg))
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Config update failed");
        }

        auto &stateMachine = server->getStateMachine();
        if (ESP_OK != stateMachine.postEvent(app::sm::APP_EVENT,
                                             app::sm::APP_EVENT_CONFIG_UPDATE))
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Posting event 'config update' failed");
        }

        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/config");
        return httpd_resp_send(req, nullptr, 0);
    }

    esp_err_t uriHandlerGetStatus(httpd_req_t *req)
    {
        if (nullptr == req)
        {
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGD(TAG, "GET /status received, content_len=%zu", req->content_len);
        ESP_RETURN_ON_ERROR(checkBasicAuth(req), TAG, "Authorization failed");

        auto *server = static_cast<HttpsServer *>(req->user_ctx);

        app::sm::SystemStatus systemStatus;
        server->getStateMachine().getSnapshot(systemStatus);

        app::cfg::AppConfig appConfig;
        server->getConfigStore().getSnapshot(appConfig);

        const std::string firmwVersEsc    = jsonEscape(app::cfg::FIRMWARE_VERSION);
        const std::string firmwBuildEsc   = jsonEscape(std::string(app::cfg::BUILD_DATE) + " " + app::cfg::BUILD_TIME);
        const std::string firmwTargetEsc  = jsonEscape(app::cfg::IDF_TARGET);
        const std::string firmwIdfVersEsc = jsonEscape(app::cfg::ESP_IDF_VERSION_STR);

        const std::string staConnectedStr = systemStatus.wifi.sta_connected ? "true" : "false";
        const std::string wifiSsidEsc     = jsonEscape(systemStatus.wifi.ssid);
        const std::string wifiHasIpStr    = systemStatus.wifi.sta_has_ip ? "true" : "false";
        const std::string wifiIpEsc       = jsonEscape(systemStatus.wifi.sta_ip);
        const std::string cfgWifiSsidEsc  = jsonEscape(appConfig.wifiMgrSta.ssid);

        const std::string otaStateEsc     = jsonEscape(otaStateToString(systemStatus.ota.state));
        const std::string otaErrStatusEsc = jsonEscape(espErrorToString(systemStatus.ota.error_status));
        const std::string otaUrlEsc       = jsonEscape(appConfig.otaMgr.url);
        const std::string otaCertEsc      = jsonEscape(appConfig.otaMgr.server_cert);

        const std::string mqttStateEsc = jsonEscape(mqttStateToString(systemStatus.mqtt.state));
        const std::string mqttUriEsc   = jsonEscape(appConfig.mqttClt.uri);
        const std::string mqttCertEsc  = jsonEscape(appConfig.mqttClt.broker_cert);

        const std::string sntpStateEsc     = jsonEscape(sntpStateToString(systemStatus.sntp.state));
        const std::string sntpValidStr     = systemStatus.sntp.is_time_valid ? "true" : "false";
        const std::string sntpErrStatusEsc = jsonEscape(espErrorToString(systemStatus.sntp.error_status));
        const std::string sntpEnabledStr   = appConfig.sntpClt.sntp_enabled ? "true" : "false";
        const std::string sntpDhcpStr      = appConfig.sntpClt.server_from_dhcp ? "true" : "false";
        const std::string sntpServer1Esc   = jsonEscape(appConfig.sntpClt.static_sntp_server[0]);
        const std::string sntpServer2Esc   = jsonEscape(appConfig.sntpClt.static_sntp_server[1]);

        const auto &deviceMgrCfg            = appConfig.deviceMgr;
        const auto &deviceMgrStatus         = systemStatus.devices;
        const std::string bme280EnabledStr  = deviceMgrCfg.hasFlag(app::dm::DeviceID::I2C_BME280) ? "true" : "false";
        const std::string bme280ActiveStr   = deviceMgrStatus.hasFlag(app::dm::DeviceID::I2C_BME280) ? "true" : "false";
        const std::string ds18b20EnabledStr = deviceMgrCfg.hasFlag(app::dm::DeviceID::ONEWIRE_DS18B20) ? "true" : "false";
        const std::string ds18b20ActiveStr  = deviceMgrStatus.hasFlag(app::dm::DeviceID::ONEWIRE_DS18B20) ? "true" : "false";
        const std::string tl136EnabledStr   = deviceMgrCfg.hasFlag(app::dm::DeviceID::MODBUS_TL136) ? "true" : "false";
        const std::string tl136ActiveStr    = deviceMgrStatus.hasFlag(app::dm::DeviceID::MODBUS_TL136) ? "true" : "false";
        const std::string bme280ValidStr    = systemStatus.bme280.sample_time_valid ? "true" : "false";
        const std::string ds18b20ValidStr   = systemStatus.ds18b20.sample_time_valid ? "true" : "false";
        const std::string tl136ValidStr     = systemStatus.tl136.sample_time_valid ? "true" : "false";

        static constexpr char STATUS_JSON_FMT[] =
            "{\n"
            "  \"firmware\": {\n"
            "    \"version\": \"%s\",\n"
            "    \"build_date_time\": \"%s\",\n"
            "    \"idf_target\": \"%s\",\n"
            "    \"esp_idf_version\": \"%s\"\n"
            "  },\n"
            "  \"wifi_status\": {\n"
            "    \"sta_connected\": %s,\n"
            "    \"ssid\": \"%s\",\n"
            "    \"last_error\": %" PRIu32 ",\n"
            "    \"sta_has_ip\": %s,\n"
            "    \"sta_ip\": \"%s\",\n"
            "    \"rssi\": %" PRId8 "\n"
            "  },\n"
            "  \"wifi\": {\n"
            "    \"ssid\": \"%s\"\n"
            "  },\n"
            "  \"ota_status\": {\n"
            "    \"state\": \"%s\",\n"
            "    \"error_status\": \"%s\"\n"
            "  },\n"
            "  \"ota\": {\n"
            "    \"url\": \"%s\",\n"
            "    \"server_cert\": \"%s\"\n"
            "  },\n"
            "  \"mqtt_status\": {\n"
            "    \"state\": \"%s\",\n"
            "    \"last_publish_us\": %" PRId64 "\n"
            "  },\n"
            "  \"mqtt\": {\n"
            "    \"uri\": \"%s\",\n"
            "    \"broker_cert\": \"%s\"\n"
            "  },\n"
            "  \"sntp_status\": {\n"
            "    \"state\": \"%s\",\n"
            "    \"is_time_valid\": %s,\n"
            "    \"last_sync_utc_us\": %" PRIu64 ",\n"
            "    \"error_status\": \"%s\"\n"
            "  },\n"
            "  \"sntp\": {\n"
            "    \"sntp_enabled\": %s,\n"
            "    \"server_from_dhcp\": %s,\n"
            "    \"static_server_1\": \"%s\",\n"
            "    \"static_server_2\": \"%s\"\n"
            "  },\n"
            "  \"devices\": {\n"
            "    \"bme280_enabled\": %s,\n"
            "    \"bme280_active\": %s,\n"
            "    \"ds18b20_enabled\": %s,\n"
            "    \"ds18b20_active\": %s,\n"
            "    \"tl136_enabled\": %s,\n"
            "    \"tl136_active\": %s\n"
            "  },\n"
            "  \"bme280_status\": {\n"
            "    \"temp_degCx100\": %" PRId32 ",\n"
            "    \"press_PaQ24d8\": %" PRIu32 ",\n"
            "    \"hum_RhQ22d10\": %" PRIu32 ",\n"
            "    \"sample_time_utc_us\": %" PRIu64 ",\n"
            "    \"sample_time_valid\": %s,\n"
            "    \"error_status\": %d\n"
            "  },\n"
            "  \"bme280\": {\n"
            "    \"device_address\": %" PRIu16 ",\n"
            "    \"probe_interval_ms\": %" PRIu32 ",\n"
            "    \"osrs_t\": %" PRIu8 ",\n"
            "    \"osrs_p\": %" PRIu8 ",\n"
            "    \"osrs_h\": %" PRIu8 "\n"
            "  },\n"
            "  \"ds18b20_status\": {\n"
            "    \"temp_degCx100\": %" PRId32 ",\n"
            "    \"sample_time_utc_us\": %" PRIu64 ",\n"
            "    \"sample_time_valid\": %s,\n"
            "    \"error_status\": %d\n"
            "  },\n"
            "  \"ds18b20\": {\n"
            "    \"address\": \"0x%016" PRIX64 "\",\n"
            "    \"probe_interval_ms\": %" PRIu32 "\n"
            "  },\n"
            "  \"tl136_status\": {\n"
            "    \"current_mAx100\": %" PRIu16 ",\n"
            "    \"water_level_mm\": %" PRIu16 ",\n"
            "    \"sample_time_utc_us\": %" PRIu64 ",\n"
            "    \"sample_time_valid\": %s,\n"
            "    \"error_status\": %d\n"
            "  },\n"
            "  \"tl136\": {\n"
            "    \"ai_converter_address\": %" PRIu8 ",\n"
            "    \"ai_converter_channel\": %" PRIu8 ",\n"
            "    \"ai_converter_calibration\": %" PRIu16 ",\n"
            "    \"probe_interval_ms\": %" PRIu32 ",\n"
            "    \"full_scale_water_level_mm\": %" PRIu16 "\n"
            "  }\n"
            "}\n";

        int len = snprintf(nullptr, 0, STATUS_JSON_FMT,
                           firmwVersEsc.c_str(),
                           firmwBuildEsc.c_str(),
                           firmwTargetEsc.c_str(),
                           firmwIdfVersEsc.c_str(),
                           staConnectedStr.c_str(),
                           wifiSsidEsc.c_str(),
                           systemStatus.wifi.last_disconnect_reason,
                           wifiHasIpStr.c_str(),
                           wifiIpEsc.c_str(),
                           systemStatus.wifi.rssi,
                           cfgWifiSsidEsc.c_str(),
                           otaStateEsc.c_str(),
                           otaErrStatusEsc.c_str(),
                           otaUrlEsc.c_str(),
                           otaCertEsc.c_str(),
                           mqttStateEsc.c_str(),
                           systemStatus.mqtt.last_publish_us,
                           mqttUriEsc.c_str(),
                           mqttCertEsc.c_str(),
                           sntpStateEsc.c_str(),
                           sntpValidStr.c_str(),
                           systemStatus.sntp.last_sync_utc_us,
                           sntpErrStatusEsc.c_str(),
                           sntpEnabledStr.c_str(),
                           sntpDhcpStr.c_str(),
                           sntpServer1Esc.c_str(),
                           sntpServer2Esc.c_str(),
                           bme280EnabledStr.c_str(),
                           bme280ActiveStr.c_str(),
                           ds18b20EnabledStr.c_str(),
                           ds18b20ActiveStr.c_str(),
                           tl136EnabledStr.c_str(),
                           tl136ActiveStr.c_str(),
                           systemStatus.bme280.data.temp_degCx100,
                           systemStatus.bme280.data.press_PaQ24d8,
                           systemStatus.bme280.data.hum_RhQ22d10,
                           systemStatus.bme280.sample_time_utc_us,
                           bme280ValidStr.c_str(),
                           systemStatus.bme280.error_status,
                           appConfig.bme280.device_address,
                           appConfig.bme280.probe_interval_ms,
                           (uint8_t)appConfig.bme280.osrs_t,
                           (uint8_t)appConfig.bme280.osrs_p,
                           (uint8_t)appConfig.bme280.osrs_h,
                           systemStatus.ds18b20.data.temp_degCx100,
                           systemStatus.ds18b20.sample_time_utc_us,
                           ds18b20ValidStr.c_str(),
                           systemStatus.ds18b20.error_status,
                           appConfig.ds18b20.address,
                           appConfig.ds18b20.probe_interval_ms,
                           systemStatus.tl136.data.current_mAx100,
                           systemStatus.tl136.data.water_level_mm,
                           systemStatus.tl136.sample_time_utc_us,
                           tl136ValidStr.c_str(),
                           systemStatus.tl136.error_status,
                           appConfig.tl136.ai_converter_address,
                           appConfig.tl136.ai_converter_channel,
                           appConfig.tl136.ai_converter_calibration,
                           appConfig.tl136.probe_interval_ms,
                           appConfig.tl136.full_scale_water_level_mm);
        if (0 > len)
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "JSON formatter failed");
        }

        auto buf = std::make_unique<char[]>((size_t)len + 1U);

        snprintf(buf.get(), (size_t)len + 1U, STATUS_JSON_FMT,
                 firmwVersEsc.c_str(),
                 firmwBuildEsc.c_str(),
                 firmwTargetEsc.c_str(),
                 firmwIdfVersEsc.c_str(),
                 staConnectedStr.c_str(),
                 wifiSsidEsc.c_str(),
                 systemStatus.wifi.last_disconnect_reason,
                 wifiHasIpStr.c_str(),
                 wifiIpEsc.c_str(),
                 systemStatus.wifi.rssi,
                 cfgWifiSsidEsc.c_str(),
                 otaStateEsc.c_str(),
                 otaErrStatusEsc.c_str(),
                 otaUrlEsc.c_str(),
                 otaCertEsc.c_str(),
                 mqttStateEsc.c_str(),
                 systemStatus.mqtt.last_publish_us,
                 mqttUriEsc.c_str(),
                 mqttCertEsc.c_str(),
                 sntpStateEsc.c_str(),
                 sntpValidStr.c_str(),
                 systemStatus.sntp.last_sync_utc_us,
                 sntpErrStatusEsc.c_str(),
                 sntpEnabledStr.c_str(),
                 sntpDhcpStr.c_str(),
                 sntpServer1Esc.c_str(),
                 sntpServer2Esc.c_str(),
                 bme280EnabledStr.c_str(),
                 bme280ActiveStr.c_str(),
                 ds18b20EnabledStr.c_str(),
                 ds18b20ActiveStr.c_str(),
                 tl136EnabledStr.c_str(),
                 tl136ActiveStr.c_str(),
                 systemStatus.bme280.data.temp_degCx100,
                 systemStatus.bme280.data.press_PaQ24d8,
                 systemStatus.bme280.data.hum_RhQ22d10,
                 systemStatus.bme280.sample_time_utc_us,
                 bme280ValidStr.c_str(),
                 systemStatus.bme280.error_status,
                 appConfig.bme280.device_address,
                 appConfig.bme280.probe_interval_ms,
                 (uint8_t)appConfig.bme280.osrs_t,
                 (uint8_t)appConfig.bme280.osrs_p,
                 (uint8_t)appConfig.bme280.osrs_h,
                 systemStatus.ds18b20.data.temp_degCx100,
                 systemStatus.ds18b20.sample_time_utc_us,
                 ds18b20ValidStr.c_str(),
                 systemStatus.ds18b20.error_status,
                 appConfig.ds18b20.address,
                 appConfig.ds18b20.probe_interval_ms,
                 systemStatus.tl136.data.current_mAx100,
                 systemStatus.tl136.data.water_level_mm,
                 systemStatus.tl136.sample_time_utc_us,
                 tl136ValidStr.c_str(),
                 systemStatus.tl136.error_status,
                 appConfig.tl136.ai_converter_address,
                 appConfig.tl136.ai_converter_channel,
                 appConfig.tl136.ai_converter_calibration,
                 appConfig.tl136.probe_interval_ms,
                 appConfig.tl136.full_scale_water_level_mm);

        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
        return httpd_resp_send(req, buf.get(), len);
    }

    esp_err_t uriHandlerPostOtaStart(httpd_req_t *req)
    {
        if (nullptr == req)
        {
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGD(TAG, "POST /ota/start received, content_len=%zu", req->content_len);
        ESP_RETURN_ON_ERROR(checkBasicAuth(req), TAG, "Authorization failed");

        if (req->content_len > 4096)
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Invalid content length");
        }

        auto body = std::make_unique<char[]>(req->content_len + 1U);
        if (ESP_OK != readRequestBody(req, body.get()))
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Body read failed");
        }

        ConfigFormData form;
        if (!parseUrlEncodedForm(body.get(), form))
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Form parse failed");
        }

        auto &server       = *static_cast<HttpsServer *>(req->user_ctx);
        auto &cfgStore     = server.getConfigStore();
        auto &stateMachine = server.getStateMachine();

        app::cfg::AppConfig appCfg;
        cfgStore.getSnapshot(appCfg);

        applyFormToConfig(form, appCfg);

        if ('\0' == appCfg.otaMgr.url[0])
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA URL missing");
        }

        if (appCfg.otaMgr.server_cert.empty())
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "OTA server certificate missing");
        }

        if (app::ota::SERVER_CERT_SIZE <= appCfg.otaMgr.server_cert.size())
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "OTA server certificate too large");
        }

        if (ESP_OK != cfgStore.update(appCfg))
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Config update failed");
        }

        if (ESP_OK != stateMachine.postEvent(app::sm::APP_EVENT,
                                             app::sm::APP_EVENT_CONFIG_UPDATE))
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Posting event 'config update' failed");
        }

        if (ESP_OK != stateMachine.postEvent(app::sm::APP_EVENT,
                                             app::sm::APP_EVENT_OTA_REQUEST))
        {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Posting event 'OTA requested' failed");
        }

        httpd_resp_set_status(req, "202 Accepted");
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
        return httpd_resp_sendstr(req, R"({"message":"OTA requested"})");
    }
}
