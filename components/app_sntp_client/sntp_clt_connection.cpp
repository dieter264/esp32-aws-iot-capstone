#include "private_include/sntp_clt_internal.hpp"

namespace app::sntp
{
    static_assert(CONFIG_LWIP_DHCP_GET_NTP_SRV,
                  "CONFIG_LWIP_DHCP_GET_NTP_SRV must be enabled for SNTP DHCP/NTP support");

    static_assert(CONFIG_LWIP_SNTP_MAX_SERVERS >= MAX_STATIC_SERVERS + 1,
                  "CONFIG_LWIP_SNTP_MAX_SERVERS is too small for DHCP NTP plus static fallback servers");

    void SntpClient::State::startClient()
    {
        if (!espSntpCltInitialized_ || espSntpCltStarted_) return;

        auto ret = esp_netif_sntp_start();
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to start SNTP");
            stateMachine_.updateSntpStatus(
                [ret](auto &status)
                {
                    status.state        = app::sntp::SntpState::ERROR;
                    status.error_status = ret;
                });

            retryStartPending_ = true;
            return;
        }

        retryStartPending_ = false;
        espSntpCltStarted_ = true;
        ESP_LOGD(TAG, "SNTP started");
        stateMachine_.updateSntpStatus(
            [](auto &status)
            {
                status.state        = app::sntp::SntpState::STARTED;
                status.error_status = ESP_OK;
            });
    }

    void SntpClient::State::createAndStartClient()
    {
        if (espSntpCltStarted_) return;

        cfgStore_.getSntpClientConfig(lastUsedSntpClientConfig_);
        if (!isAppSntpConfigAvailable(lastUsedSntpClientConfig_))
        {
            app::sntp::SntpClientStatus sntpStatus{};
            stateMachine_.getSntpStatus(sntpStatus);
            if (app::sntp::SntpState::NOT_CONFIGURED != sntpStatus.state)
            {
                ESP_LOGI(TAG, "SNTP Client not configured");
                sntpStatus       = {};
                sntpStatus.state = app::sntp::SntpState::NOT_CONFIGURED;
                stateMachine_.updateSntpStatus(
                    [sntpStatus](auto &status)
                    {
                        status = sntpStatus;
                    });
            }

            return;
        }

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0, {});
        config.start             = false;
        config.wait_for_sync     = false;

        if (lastUsedSntpClientConfig_.server_from_dhcp)
        {
            config.server_from_dhcp           = true;
            config.index_of_first_server      = 1;
            config.renew_servers_after_new_IP = true;
        }

        size_t serverCount = 0;
        for (size_t i = 0; i < app::sntp::MAX_STATIC_SERVERS; i++)
        {
            if ('\0' != lastUsedSntpClientConfig_.static_sntp_server[i][0])
            {
                config.servers[serverCount++] =
                    lastUsedSntpClientConfig_.static_sntp_server[i];
            }
        }
        config.num_of_servers = serverCount;

        auto ret = esp_netif_sntp_init(&config);
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "Failed to initialize SNTP");
            stateMachine_.updateSntpStatus(
                [ret](auto &status)
                {
                    status.state        = app::sntp::SntpState::ERROR;
                    status.error_status = ret;
                });

            return;
        }

        ESP_LOGD(TAG, "SNTP initialized");
        espSntpCltInitialized_ = true;
        stateMachine_.updateSntpStatus(
            [](auto &status)
            {
                status.state        = app::sntp::SntpState::INITIALIZED;
                status.error_status = ESP_OK;
            });

        app::wm::WifiStatus wifiStatus{};
        stateMachine_.getWifiStatus(wifiStatus);
        if (wifiStatus.sta_has_ip)
        {
            startClient();
        }
    }

    void SntpClient::State::destroyClient()
    {
        if (!espSntpCltInitialized_) return;

        esp_netif_sntp_deinit();

        espSntpCltInitialized_    = false;
        retryStartPending_        = false;
        lastUsedSntpClientConfig_ = {};
        espSntpCltStarted_        = false;
        ESP_LOGD(TAG, "SNTP deinitialized");
        stateMachine_.updateSntpStatus(
            [](auto &status)
            {
                status.state = app::sntp::SntpState::STOPPED;
            });
    }

    bool SntpClient::State::isAppSntpConfigAvailable(const SntpClientConfig &cfg)
    {
        if (cfg.sntp_enabled)
        {
            if (cfg.server_from_dhcp || ('\0' != cfg.static_sntp_server[0][0]))
            {
                return true;
            }
        }

        return false;
    }
}
