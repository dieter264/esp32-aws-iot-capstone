#include "esp_tls_crypto.h"
#include "https_srv_internal.hpp"

namespace app::https
{
    esp_err_t checkBasicAuth(httpd_req_t *);
    bool validateAuthCreds(httpd_req_t *req, const char *creds, size_t credsCap);

    esp_err_t checkBasicAuth(httpd_req_t *req)
    {
        size_t authCredsCap = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
        if (1 == authCredsCap)
        {
            ESP_LOGD(TAG, "No auth header received");
            httpd_resp_set_status(req, "401 UNAUTHORIZED");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32 Admin\"");
            return httpd_resp_send(req, NULL, 0);
        }

        auto authCreds = std::make_unique<char[]>(authCredsCap);
        LOGD_CALL_RET_ON_ERR(TAG, httpd_req_get_hdr_value_str,
                             (req, "Authorization", authCreds.get(), authCredsCap),
                             "Authorization value received");

        if (!validateAuthCreds(req, authCreds.get(), authCredsCap))
        {
            ESP_LOGD(TAG, "Invalid username/password");
            httpd_resp_set_status(req, "401 UNAUTHORIZED");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32 Admin\"");
            return httpd_resp_send(req, NULL, 0);
        }

        auto *httpsServer = static_cast<HttpsServer *>(req->user_ctx);
        app::https::HttpsServerConfig httpsSrvCfg;
        httpsServer->getConfigStore().getHttpsServerConfig(httpsSrvCfg);
        const char *username = httpsSrvCfg.username;
        ESP_LOGD(TAG, "Username '%s' authorized", username);
        return ESP_OK;
    }

    bool validateAuthCreds(httpd_req_t *req, const char *creds, size_t credsCap)
    {
        auto *httpsServer = static_cast<HttpsServer *>(req->user_ctx);
        app::https::HttpsServerConfig httpsSrvCfg;
        httpsServer->getConfigStore().getHttpsServerConfig(httpsSrvCfg);
        const char *username = httpsSrvCfg.username;
        const char *password = httpsSrvCfg.password;

        const size_t bufCap = strlen(username) + 1 + strlen(password) + 1;
        auto buf            = std::make_unique<char[]>(bufCap);
        snprintf(buf.get(), bufCap, "%s:%s", username, password);
        size_t refCredsCap = 0;

        esp_crypto_base64_encode(nullptr, 0, &refCredsCap,
                                 (const uint8_t *)buf.get(),
                                 strlen(buf.get()));

        const auto offset = strlen("Basic ");
        auto refCreds     = std::make_unique<char[]>(offset + refCredsCap);
        strcpy(refCreds.get(), "Basic ");
        size_t olen = 0;

        if (0 != esp_crypto_base64_encode((uint8_t *)&refCreds[offset],
                                          refCredsCap, &olen,
                                          (const uint8_t *)buf.get(),
                                          strlen(buf.get())))
        {
            return false;
        }

        refCreds[offset + olen] = 0;
        return (0 == strncmp(refCreds.get(), creds, credsCap));
    }
}
