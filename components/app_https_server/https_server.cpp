#include "https_srv_internal.hpp"

namespace app::https
{
    constexpr auto HTTPS_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_HTTPS_LOG_LEVEL);

    extern const unsigned char SERVER_CERT_START[] asm("_binary_server_crt_start");
    extern const unsigned char SERVER_CERT_END[] asm("_binary_server_crt_end");
    extern const unsigned char SERVER_KEY_START[] asm("_binary_server_key_start");
    extern const unsigned char SERVER_KEY_END[] asm("_binary_server_key_end");

    void apply_log_level()
    {
        if (-1 < HTTPS_LOG_LEVEL)
        {
            esp_log_level_set(TAG, HTTPS_LOG_LEVEL);
        }
    }

    struct HttpsServer::State
    {
        httpd_handle_t httpsServer = nullptr;

        httpd_uri_t httpUriGetCfg{
            .uri      = "/config",
            .method   = HTTP_GET,
            .handler  = uriHandlerGetConfig,
            .user_ctx = nullptr};

        httpd_uri_t httpUriPostCfg{
            .uri      = "/config",
            .method   = HTTP_POST,
            .handler  = uriHandlerPostConfig,
            .user_ctx = nullptr};

        httpd_uri_t httpUriGetStat{
            .uri      = "/status",
            .method   = HTTP_GET,
            .handler  = uriHandlerGetStatus,
            .user_ctx = nullptr};

        httpd_uri_t httpUriPostOtaStart{
            .uri      = "/ota/start",
            .method   = HTTP_POST,
            .handler  = uriHandlerPostOtaStart,
            .user_ctx = nullptr};
    };

    HttpsServer::HttpsServer(app::cfg::ConfigStore &store, app::sm::StateMachine &sm)
        : cfgStore_(store), stateMachine_(sm), state_(std::make_unique<State>())
    {
        state_->httpUriGetCfg.user_ctx       = this;
        state_->httpUriPostCfg.user_ctx      = this;
        state_->httpUriGetStat.user_ctx      = this;
        state_->httpUriPostOtaStart.user_ctx = this;
    }

    HttpsServer::~HttpsServer()
    {
        fini();
    }

    esp_err_t HttpsServer::init()
    {
        if (isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        httpd_ssl_config_t sslConf = HTTPD_SSL_CONFIG_DEFAULT();
        sslConf.servercert         = SERVER_CERT_START;
        sslConf.servercert_len     = SERVER_CERT_END - SERVER_CERT_START;
        sslConf.prvtkey_pem        = SERVER_KEY_START;
        sslConf.prvtkey_len        = SERVER_KEY_END - SERVER_KEY_START;

        esp_err_t ret = ESP_FAIL;

        LOGD_CALL(TAG, ret, httpd_ssl_start,
                  (&(state_->httpsServer), &sslConf),
                  "HTTPS Server started");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, httpd_register_uri_handler,
                  (state_->httpsServer, &(state_->httpUriGetCfg)),
                  "URI handler 'GET /config' registered");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, httpd_register_uri_handler,
                  (state_->httpsServer, &(state_->httpUriPostCfg)),
                  "URI handler 'POST /config' registered");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, httpd_register_uri_handler,
                  (state_->httpsServer, &(state_->httpUriGetStat)),
                  "URI handler 'GET /status' registered");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, httpd_register_uri_handler,
                  (state_->httpsServer, &(state_->httpUriPostOtaStart)),
                  "URI handler 'POST /ota/start' registered");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        isInitialized_ = true;
        ESP_LOGD(TAG, "HTTPS Server initialized");
        return ESP_OK;
    }

    void HttpsServer::fini()
    {
        if (nullptr != state_->httpsServer)
        {
            httpd_ssl_stop(state_->httpsServer);
            state_->httpsServer = nullptr;
        }

        if (!isInitialized_) return;
        isInitialized_ = false;
        ESP_LOGD(TAG, "HTTPS Server finalized");
    }

    esp_err_t HttpsServer::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset HTTPS Server");

        ESP_LOGD(TAG, "HTTPS Server reset");
        return ESP_OK;
    }

    bool HttpsServer::isInitialized() const
    {
        return isInitialized_;
    }

    app::cfg::ConfigStore &HttpsServer::getConfigStore()
    {
        return cfgStore_;
    }

    app::sm::StateMachine &HttpsServer::getStateMachine()
    {
        return stateMachine_;
    }
}
