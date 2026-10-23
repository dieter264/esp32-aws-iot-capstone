#pragma once

#include <inttypes.h>
#include <string>
#include "esp_check.h"
#include <esp_https_server.h>
#include "app/cmn/log.hpp"
#include "app/cfg/config_store.hpp"
#include "app/cfg/firmware.hpp"
#include "app/sm/state_machine.hpp"
#include "app/https/https_server.hpp"

namespace app::https
{
    const char *const TAG = "app_https";

    esp_err_t checkBasicAuth(httpd_req_t *);

    esp_err_t uriHandlerGetConfig(httpd_req_t *);
    esp_err_t uriHandlerPostConfig(httpd_req_t *);
    esp_err_t uriHandlerGetStatus(httpd_req_t *);
    esp_err_t uriHandlerPostOtaStart(httpd_req_t *);

    struct ConfigFormData
    {
        bool wifiSsidPresent                  = false;
        char wifiSsid[app::wm::SSID_BUF_SIZE] = {};

        bool wifiPassPresent                  = false;
        char wifiPass[app::wm::PASS_BUF_SIZE] = {};

        bool otaUrlPresent              = false;
        char otaUrl[app::ota::URL_SIZE] = {};

        bool otaServerCertPresent = false;
        std::string otaServerCert = {};

        bool mqttUriPresent               = false;
        char mqttUri[app::mqtt::URI_SIZE] = {};

        bool mqttBrokerCertPresent = false;
        std::string mqttBrokerCert = {};

        bool sntpEnabledPresent = false;
        bool sntpEnabled        = false;

        bool sntpFromDhcpPresent = false;
        bool sntpFromDhcp        = false;

        bool sntpStaticServer1Present                            = false;
        char sntpStaticServer1[app::sntp::SNTP_SERVER_NAME_SIZE] = {};

        bool sntpStaticServer2Present                            = false;
        char sntpStaticServer2[app::sntp::SNTP_SERVER_NAME_SIZE] = {};

        bool bme280EnabledPresent = false;
        bool bme280Enabled        = false;

        bool ds18b20EnabledPresent = false;
        bool ds18b20Enabled        = false;

        bool tl136EnabledPresent = false;
        bool tl136Enabled        = false;

        bool bme280AddrPresent = false;
        uint16_t bme280Addr    = 0;

        bool bme280ProbeIntervalPresent = false;
        uint32_t bme280ProbeIntervalMs  = 0;

        bool bme280OsrsTPresent = false;
        uint8_t bme280OsrsT     = 0;

        bool bme280OsrsPPresent = false;
        uint8_t bme280OsrsP     = 0;

        bool bme280OsrsHPresent = false;
        uint8_t bme280OsrsH     = 0;

        bool ds18b20AddressPresent = false;
        uint64_t ds18b20Address    = 0;

        bool ds18b20ProbeIntervalPresent = false;
        uint32_t ds18b20ProbeIntervalMs  = 0;

        bool tl136AiConverterAddressPresent = false;
        uint8_t tl136AiConverterAddress      = 0;

        bool tl136AiConverterChannelPresent = false;
        uint8_t tl136AiConverterChannel      = 0;

        bool tl136AiConverterCalibrationPresent = false;
        uint16_t tl136AiConverterCalibration    = 0;

        bool tl136ProbeIntervalPresent = false;
        uint32_t tl136ProbeIntervalMs  = 0;

        bool tl136FullScaleWaterLevelPresent = false;
        uint16_t tl136FullScaleWaterLevelMm  = 0;
    };

    esp_err_t readRequestBody(httpd_req_t *, char *buf);
    bool parseUrlEncodedForm(char *body, ConfigFormData &);
    void applyFormToConfig(const ConfigFormData &, app::cfg::AppConfig &);
    const char *otaStateToString(app::ota::OtaState);
    const char *mqttStateToString(app::mqtt::MqttState);
    const char *sntpStateToString(app::sntp::SntpState);

    std::string jsonEscape(const char *);
    std::string jsonEscape(const std::string &);

    const char *espErrorToString(int err_t);
}
