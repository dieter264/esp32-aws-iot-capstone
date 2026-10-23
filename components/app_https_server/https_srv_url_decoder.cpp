#include "https_srv_internal.hpp"

namespace app::https
{
    esp_err_t readRequestBody(httpd_req_t *req, char *buf)
    {
        const auto len      = req->content_len;
        size_t totalRead    = 0;
        uint8_t numTimeouts = 10;
        while (len > totalRead)
        {
            int ret = httpd_req_recv(req, buf + totalRead, len - totalRead);

            if (HTTPD_SOCK_ERR_TIMEOUT == ret && 0u < numTimeouts--)
            {
                continue;
            }

            if (ret <= 0)
            {
                return ESP_FAIL;
            }

            totalRead += (size_t)ret;
        }

        buf[len] = '\0';
        return ESP_OK;
    }

    bool hexToNibble(char c, uint8_t &nibble)
    {
        if ('0' <= c && c <= '9')
        {
            nibble = (uint8_t)(c - '0');
            return true;
        }
        if ('A' <= c && c <= 'F')
        {
            nibble = (uint8_t)(10 + (c - 'A'));
            return true;
        }
        if ('a' <= c && c <= 'f')
        {
            nibble = (uint8_t)(10 + (c - 'a'));
            return true;
        }

        return false;
    }

    bool urlDecodeInPlace(char *s)
    {
        char *src = s;
        char *dst = s;

        while ('\0' != *src)
        {
            if ('+' == *src)
            {
                *dst++ = ' ';
                ++src;
            }
            else if ('%' == *src)
            {
                if ('\0' == src[1] || '\0' == src[2])
                {
                    return false;
                }

                uint8_t hi = 0, lo = 0;
                if (!hexToNibble(src[1], hi) || !hexToNibble(src[2], lo))
                {
                    return false;
                }

                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            }
            else
            {
                *dst++ = *src++;
            }
        }

        *dst = '\0';
        return true;
    }

    bool parseUint32(const char *s, uint32_t minVal, uint32_t maxVal, uint32_t &out)
    {
        if ('\0' == *s)
        {
            return false;
        }

        uint32_t v = 0U;
        for (const char *p = s; '\0' != *p; ++p)
        {
            if (*p < '0' || *p > '9')
            {
                return false;
            }
            const uint32_t digit = (uint32_t)(*p - '0');
            if (v > (UINT32_MAX - digit) / 10U)
            {
                return false;
            }
            v = (10U * v) + digit;
        }

        if (v < minVal || v > maxVal)
        {
            return false;
        }
        out = v;
        return true;
    }

    bool parseUint64Hex(const char *s, uint64_t &out)
    {
        if ('0' == s[0] && ('x' == s[1] || 'X' == s[1]))
        {
            s += 2;
        }

        if ('\0' == *s)
        {
            return false;
        }

        uint64_t v = 0U;
        for (const char *p = s; '\0' != *p; ++p)
        {
            uint8_t digit = 0;
            if (!hexToNibble(*p, digit) || v > ((UINT64_MAX - digit) >> 4U))
            {
                return false;
            }
            v = (v << 4U) | digit;
        }

        out = v;
        return true;
    }

    bool parseConfigField(const char *key, const char *value, ConfigFormData &form)
    {
        uint32_t v = 0;

        if (0 == strcmp(key, "wifi_ssid"))
        {
            form.wifiSsidPresent = true;
            return app::cfg::cstrCopy(form.wifiSsid, value);
        }

        if (0 == strcmp(key, "wifi_pass"))
        {
            form.wifiPassPresent = true;
            return app::cfg::cstrCopy(form.wifiPass, value);
        }

        if (0 == strcmp(key, "ota_url"))
        {
            form.otaUrlPresent = true;
            return app::cfg::cstrCopy(form.otaUrl, value);
        }

        if (0 == strcmp(key, "ota_server_cert"))
        {
            form.otaServerCertPresent = true;

            if (app::ota::SERVER_CERT_SIZE <= strlen(value))
            {
                return false;
            }

            form.otaServerCert = value;
            return true;
        }

        if (0 == strcmp(key, "mqtt_uri"))
        {
            form.mqttUriPresent = true;
            return app::cfg::cstrCopy(form.mqttUri, value);
        }

        if (0 == strcmp(key, "mqtt_broker_cert"))
        {
            form.mqttBrokerCertPresent = true;

            if (app::mqtt::BROKER_CERT_SIZE <= strlen(value))
            {
                return false;
            }

            form.mqttBrokerCert = value;
            return true;
        }

        if (0 == strcmp(key, "sntp_enabled"))
        {
            form.sntpEnabledPresent = true;
            form.sntpEnabled        = (0 == strcmp(value, "1"));
            return true;
        }

        if (0 == strcmp(key, "sntp_server_from_dhcp"))
        {
            form.sntpFromDhcpPresent = true;
            form.sntpFromDhcp        = (0 == strcmp(value, "1"));
            return true;
        }

        if (0 == strcmp(key, "sntp_static_server_1"))
        {
            form.sntpStaticServer1Present = true;
            return app::cfg::cstrCopy(form.sntpStaticServer1, value);
        }

        if (0 == strcmp(key, "sntp_static_server_2"))
        {
            form.sntpStaticServer2Present = true;
            return app::cfg::cstrCopy(form.sntpStaticServer2, value);
        }

        if (0 == strcmp(key, "bme280_enabled"))
        {
            form.bme280EnabledPresent = true;
            form.bme280Enabled        = (0 == strcmp(value, "1"));
            return true;
        }

        if (0 == strcmp(key, "ds18b20_enabled"))
        {
            form.ds18b20EnabledPresent = true;
            form.ds18b20Enabled        = (0 == strcmp(value, "1"));
            return true;
        }

        if (0 == strcmp(key, "tl136_enabled"))
        {
            form.tl136EnabledPresent = true;
            form.tl136Enabled        = (0 == strcmp(value, "1"));
            return true;
        }

        if (0 == strcmp(key, "bme280_addr"))
        {
            if (!parseUint32(value, 0U, 255U, v))
            {
                return false;
            }
            if (!(0U == v || 118U == v || 119U == v))
            {
                return false;
            }

            form.bme280AddrPresent = true;
            form.bme280Addr        = (uint16_t)v;
            return true;
        }

        if (0 == strcmp(key, "bme280_probe_interval_ms"))
        {
            if (!parseUint32(value, 1000U, 3600000U, v))
            {
                return false;
            }
            form.bme280ProbeIntervalPresent = true;
            form.bme280ProbeIntervalMs      = v;
            return true;
        }

        if (0 == strcmp(key, "bme280_osrs_t"))
        {
            if (!parseUint32(value, 0U, 5U, v))
            {
                return false;
            }
            form.bme280OsrsTPresent = true;
            form.bme280OsrsT        = (uint8_t)v;
            return true;
        }

        if (0 == strcmp(key, "bme280_osrs_p"))
        {
            if (!parseUint32(value, 0U, 5U, v))
            {
                return false;
            }
            form.bme280OsrsPPresent = true;
            form.bme280OsrsP        = (uint8_t)v;
            return true;
        }

        if (0 == strcmp(key, "bme280_osrs_h"))
        {
            if (!parseUint32(value, 0U, 5U, v))
            {
                return false;
            }
            form.bme280OsrsHPresent = true;
            form.bme280OsrsH        = (uint8_t)v;
            return true;
        }

        if (0 == strcmp(key, "ds18b20_address"))
        {
            uint64_t address = 0;
            if (!parseUint64Hex(value, address))
            {
                return false;
            }
            form.ds18b20AddressPresent = true;
            form.ds18b20Address        = address;
            return true;
        }

        if (0 == strcmp(key, "ds18b20_probe_interval_ms"))
        {
            if (!parseUint32(value, 1000U, 3600000U, v))
            {
                return false;
            }
            form.ds18b20ProbeIntervalPresent = true;
            form.ds18b20ProbeIntervalMs      = v;
            return true;
        }

        if (0 == strcmp(key, "tl136_ai_converter_address"))
        {
            if (!parseUint32(value, 1U, 247U, v))
            {
                return false;
            }
            form.tl136AiConverterAddressPresent = true;
            form.tl136AiConverterAddress        = static_cast<uint8_t>(v);
            return true;
        }

        if (0 == strcmp(key, "tl136_ai_converter_channel"))
        {
            if (!parseUint32(value, 1U, 2U, v))
            {
                return false;
            }
            form.tl136AiConverterChannelPresent = true;
            form.tl136AiConverterChannel        = static_cast<uint8_t>(v);
            return true;
        }

        if (0 == strcmp(key, "tl136_ai_converter_calibration"))
        {
            if (!parseUint32(value, 1U, 65535U, v))
            {
                return false;
            }
            form.tl136AiConverterCalibrationPresent = true;
            form.tl136AiConverterCalibration        = static_cast<uint16_t>(v);
            return true;
        }

        if (0 == strcmp(key, "tl136_probe_interval_ms"))
        {
            if (!parseUint32(value, 1000U, 3600000U, v))
            {
                return false;
            }
            form.tl136ProbeIntervalPresent = true;
            form.tl136ProbeIntervalMs      = v;
            return true;
        }

        if (0 == strcmp(key, "tl136_full_scale_water_level_mm"))
        {
            if (!parseUint32(value, 1U, 65535U, v))
            {
                return false;
            }
            form.tl136FullScaleWaterLevelPresent = true;
            form.tl136FullScaleWaterLevelMm      = static_cast<uint16_t>(v);
            return true;
        }

        return true; // ignore unknown keys
    }

    bool parseUrlEncodedForm(char *body, ConfigFormData &form)
    {
        while ('\0' != *body)
        {
            // Split current field from the rest
            char *amp = strchr(body, '&');
            if (NULL != amp)
            {
                *amp = '\0';
            }

            // Split key and value
            char *eq = strchr(body, '=');
            if (NULL == eq)
            {
                return false;
            }

            *eq = '\0';

            char *key   = body;
            char *value = eq + 1;

            if (!urlDecodeInPlace(key) || !urlDecodeInPlace(value))
            {
                return false;
            }

            if (!parseConfigField(key, value, form))
            {
                return false;
            }

            if (NULL == amp)
            {
                break;
            }

            body = amp + 1;
        }

        return true;
    }

    void applyFormToConfig(const ConfigFormData &form, app::cfg::AppConfig &cfg)
    {
        if (form.wifiSsidPresent)
        {
            app::cfg::cstrCopy(cfg.wifiMgrSta.ssid, form.wifiSsid);
        }

        if (form.wifiPassPresent && '\0' != form.wifiPass[0])
        {
            app::cfg::cstrCopy(cfg.wifiMgrSta.password, form.wifiPass);
        }

        if (form.otaUrlPresent)
        {
            app::cfg::cstrCopy(cfg.otaMgr.url, form.otaUrl);
        }

        if (form.otaServerCertPresent && !form.otaServerCert.empty())
        {
            cfg.otaMgr.server_cert = form.otaServerCert;
        }

        if (form.mqttUriPresent)
        {
            app::cfg::cstrCopy(cfg.mqttClt.uri, form.mqttUri);
        }

        if (form.mqttBrokerCertPresent && !form.mqttBrokerCert.empty())
        {
            cfg.mqttClt.broker_cert = form.mqttBrokerCert;
        }

        if (form.sntpEnabledPresent)
        {
            if (!form.sntpEnabled)
            {
                cfg.sntpClt = {};
            }
            else
            {
                cfg.sntpClt.sntp_enabled = true;

                if (form.sntpFromDhcpPresent)
                {
                    cfg.sntpClt.server_from_dhcp = form.sntpFromDhcp;
                }

                if (form.sntpStaticServer1Present)
                {
                    app::cfg::cstrCopy(cfg.sntpClt.static_sntp_server[0],
                                       form.sntpStaticServer1);

                    if (form.sntpStaticServer2Present)
                    {
                        if ('\0' != form.sntpStaticServer1[0])
                        {
                            app::cfg::cstrCopy(cfg.sntpClt.static_sntp_server[1],
                                               form.sntpStaticServer2);
                        }
                        else
                        {
                            cfg.sntpClt.static_sntp_server[1][0] = '\0';
                        }
                    }
                }
            }
        }

        if (form.bme280EnabledPresent)
        {
            if (form.bme280Enabled)
            {
                cfg.deviceMgr.setFlag(app::dm::DeviceID::I2C_BME280);
            }
            else
            {
                cfg.deviceMgr.rmFlag(app::dm::DeviceID::I2C_BME280);
            }
        }

        if (form.ds18b20EnabledPresent)
        {
            if (form.ds18b20Enabled)
            {
                cfg.deviceMgr.setFlag(app::dm::DeviceID::ONEWIRE_DS18B20);
            }
            else
            {
                cfg.deviceMgr.rmFlag(app::dm::DeviceID::ONEWIRE_DS18B20);
            }
        }

        if (form.tl136EnabledPresent)
        {
            if (form.tl136Enabled)
            {
                cfg.deviceMgr.setFlag(app::dm::DeviceID::MODBUS_TL136);
            }
            else
            {
                cfg.deviceMgr.rmFlag(app::dm::DeviceID::MODBUS_TL136);
            }
        }

        if (form.bme280AddrPresent)
        {
            cfg.bme280.device_address = form.bme280Addr;
        }

        if (form.bme280ProbeIntervalPresent)
        {
            cfg.bme280.probe_interval_ms = form.bme280ProbeIntervalMs;
        }

        if (form.bme280OsrsTPresent)
        {
            cfg.bme280.osrs_t = static_cast<app::i2c::bme280::Oversampling>(form.bme280OsrsT);
        }

        if (form.bme280OsrsPPresent)
        {
            cfg.bme280.osrs_p = static_cast<app::i2c::bme280::Oversampling>(form.bme280OsrsP);
        }

        if (form.bme280OsrsHPresent)
        {
            cfg.bme280.osrs_h = static_cast<app::i2c::bme280::Oversampling>(form.bme280OsrsH);
        }

        if (form.ds18b20AddressPresent)
        {
            cfg.ds18b20.address = form.ds18b20Address;
        }

        if (form.ds18b20ProbeIntervalPresent)
        {
            cfg.ds18b20.probe_interval_ms = form.ds18b20ProbeIntervalMs;
        }

        if (form.tl136AiConverterAddressPresent)
        {
            cfg.tl136.ai_converter_address = form.tl136AiConverterAddress;
        }

        if (form.tl136AiConverterChannelPresent)
        {
            cfg.tl136.ai_converter_channel = form.tl136AiConverterChannel;
        }

        if (form.tl136AiConverterCalibrationPresent)
        {
            cfg.tl136.ai_converter_calibration = form.tl136AiConverterCalibration;
        }

        if (form.tl136ProbeIntervalPresent)
        {
            cfg.tl136.probe_interval_ms = form.tl136ProbeIntervalMs;
        }

        if (form.tl136FullScaleWaterLevelPresent)
        {
            cfg.tl136.full_scale_water_level_mm = form.tl136FullScaleWaterLevelMm;
        }
    }

    const char *otaStateToString(app::ota::OtaState state)
    {
        switch (state)
        {
        case app::ota::OtaState::IDLE:
            return "IDLE";
        case app::ota::OtaState::REQUESTED:
            return "REQUESTED";
        case app::ota::OtaState::RUNNING:
            return "RUNNING";
        case app::ota::OtaState::FAILED:
            return "FAILED";
        case app::ota::OtaState::RESTART_PENDING:
            return "RESTART_PENDING";
        default:
            return "UNKNOWN";
        }
    }

    const char *mqttStateToString(app::mqtt::MqttState state)
    {
        switch (state)
        {
        case app::mqtt::MqttState::UNKNOWN:
            return "UNKNOWN";
        case app::mqtt::MqttState::NOT_CONFIGURED:
            return "NOT_CONFIGURED";
        case app::mqtt::MqttState::STOPPED:
            return "STOPPED";
        case app::mqtt::MqttState::CONNECTING:
            return "CONNECTING";
        case app::mqtt::MqttState::CONNECTED:
            return "CONNECTED";
        case app::mqtt::MqttState::PUBLISHED:
            return "PUBLISHED";
        case app::mqtt::MqttState::DISCONNECTED:
            return "DISCONNECTED";
        case app::mqtt::MqttState::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    const char *sntpStateToString(app::sntp::SntpState state)
    {
        switch (state)
        {
        case app::sntp::SntpState::UNKNOWN:
            return "UNKNOWN";
        case app::sntp::SntpState::NOT_CONFIGURED:
            return "NOT_CONFIGURED";
        case app::sntp::SntpState::INITIALIZED:
            return "INITIALIZED";
        case app::sntp::SntpState::STARTED:
            return "STARTED";
        case app::sntp::SntpState::SYNCED:
            return "SYNCED";
        case app::sntp::SntpState::STOPPED:
            return "STOPPED";
        case app::sntp::SntpState::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    std::string jsonEscape(const char *s)
    {
        std::string out;
        if (nullptr == s)
            return out;

        while (*s != '\0')
        {
            switch (*s)
            {
            case '\\':
                out += "\\\\";
                break;
            case '\"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += *s;
                break;
            }
            ++s;
        }

        return out;
    }

    std::string jsonEscape(const std::string &s)
    {
        return jsonEscape(s.c_str());
    }

    const char *espErrorToString(int err_t)
    {
        return esp_err_to_name(static_cast<esp_err_t>(err_t));
    }
}
