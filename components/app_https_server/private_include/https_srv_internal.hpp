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

        bool bmeAddrPresent = false;
        uint16_t bmeAddr    = 0;

        bool bmeOsrsTPresent = false;
        uint8_t bmeOsrsT     = 0;

        bool bmeOsrsPPresent = false;
        uint8_t bmeOsrsP     = 0;

        bool bmeOsrsHPresent = false;
        uint8_t bmeOsrsH     = 0;
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
