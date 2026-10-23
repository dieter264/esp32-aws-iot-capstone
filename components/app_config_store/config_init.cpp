#include "private_include/config_internal.hpp"

namespace app::cfg
{
    void ConfigStore::State::initWifiManagerApConfig()
    {
        auto &wmApCfg = appConfig_.wifiMgrAP;

        const auto ssid_len = sizeof(CONFIG_APP_WM_AP_SSID) - 1;
        static_assert(1 <= ssid_len && app::wm::SSID_BUF_SIZE > ssid_len);
        cstrCopy(wmApCfg.ssid, CONFIG_APP_WM_AP_SSID);

        const auto passwd_len = sizeof(CONFIG_APP_WM_AP_PASSWD) - 1;
        static_assert(8 <= passwd_len && app::wm::PASS_BUF_SIZE > passwd_len);
        cstrCopy(wmApCfg.password, CONFIG_APP_WM_AP_PASSWD);

        appConfig_.wifiMgrAP.channel = CONFIG_APP_WM_AP_CHANNEL;
    }

    void ConfigStore::State::initHttpsServerConfig()
    {
        auto &httpsSrvCfg = appConfig_.httpsSrv;

        const auto user_len = sizeof(CONFIG_APP_HTTPS_LOGIN_USER) - 1;
        static_assert(1 <= user_len && app::https::USER_BUF_SIZE > user_len);
        cstrCopy(httpsSrvCfg.username, CONFIG_APP_HTTPS_LOGIN_USER);

        const auto passwd_len = sizeof(CONFIG_APP_HTTPS_LOGIN_PASSWD) - 1;
        static_assert(8 <= passwd_len && app::https::PASS_BUF_SIZE > passwd_len);
        cstrCopy(httpsSrvCfg.password, CONFIG_APP_HTTPS_LOGIN_PASSWD);
    }

    void ConfigStore::State::initSntpClientConfig()
    {
#ifdef CONFIG_APP_SNTP_ENABLED
        auto &sntpCltCfg        = appConfig_.sntpClt;
        sntpCltCfg.sntp_enabled = true;

#ifdef CONFIG_APP_SNTP_SERVER_FROM_DHCP
        constexpr bool SNTP_SERVER_FROM_DHCP = true;
        sntpCltCfg.server_from_dhcp          = true;
#else
        constexpr bool SNTP_SERVER_FROM_DHCP = false;
#endif

        constexpr auto s1_len       = sizeof(CONFIG_APP_SNTP_STATIC_SERVER_1) - 1;
        constexpr bool dhcpOrStatic = (SNTP_SERVER_FROM_DHCP || 1 <= s1_len);
        static_assert(dhcpOrStatic && app::sntp::SNTP_SERVER_NAME_SIZE > s1_len);
        cstrCopy(sntpCltCfg.static_sntp_server[0], CONFIG_APP_SNTP_STATIC_SERVER_1);

        constexpr auto s2_len = sizeof(CONFIG_APP_SNTP_STATIC_SERVER_2) - 1;
        static_assert(app::sntp::SNTP_SERVER_NAME_SIZE > s2_len);
        cstrCopy(sntpCltCfg.static_sntp_server[1], CONFIG_APP_SNTP_STATIC_SERVER_2);

#endif
    }

    void ConfigStore::State::initDeviceManagerConfig()
    {
#ifdef CONFIG_APP_DM_BME280_ENABLED
        appConfig_.deviceMgr.setFlag(app::dm::DeviceID::I2C_BME280);
#endif

#ifdef CONFIG_APP_DM_DS18B20_ENABLED
        appConfig_.deviceMgr.setFlag(app::dm::DeviceID::ONEWIRE_DS18B20);
#endif

#ifdef CONFIG_APP_DM_TL136_ENABLED
        appConfig_.deviceMgr.setFlag(app::dm::DeviceID::MODBUS_TL136);
#endif
    }

    void ConfigStore::State::initBme280Config()
    {
        appConfig_.bme280.probe_interval_ms = CONFIG_APP_BME280_PROBE_INTERVAL_MS;
    }

    void ConfigStore::State::initOneWireBusConfig()
    {
        appConfig_.oneWireBus.gpio_num     = CONFIG_APP_ONEWIRE_BUS_GPIO;
        appConfig_.oneWireBus.max_rx_bytes = CONFIG_APP_ONEWIRE_BUS_MAX_RX_BYTES;
    }

    void ConfigStore::State::initDs18b20Config()
    {
        appConfig_.ds18b20.address           = CONFIG_APP_ONEWIRE_DS18B20_ADDRESS;
        appConfig_.ds18b20.probe_interval_ms = CONFIG_APP_ONEWIRE_DS18B20_PROBE_INTERVAL_MS;
    }

    void ConfigStore::State::initModbusBusConfig()
    {
        appConfig_.modbusBus.uart_port    = CONFIG_APP_MODBUS_UART_PORT;
        appConfig_.modbusBus.uart_tx_gpio = CONFIG_APP_MODBUS_UART_TX_GPIO;
        appConfig_.modbusBus.uart_rx_gpio = CONFIG_APP_MODBUS_UART_RX_GPIO;
        appConfig_.modbusBus.uart_rs485_transceiver =
            static_cast<app::modbus::bus::UART_RS485_Transceiver>(CONFIG_APP_MODBUS_UART_RS485_TRANSCEIVER);
        if (app::modbus::bus::MAX485 == appConfig_.modbusBus.uart_rs485_transceiver)
        {
#ifdef CONFIG_APP_MODBUS_UART_RTS_GPIO
            appConfig_.modbusBus.uart_rts_gpio = CONFIG_APP_MODBUS_UART_RTS_GPIO;
#endif
        }
        appConfig_.modbusBus.uart_baud_rate   = CONFIG_APP_MODBUS_UART_BAUD_RATE;
        appConfig_.modbusBus.response_tout_ms = CONFIG_APP_MODBUS_RESPONSE_TOUT_MS;
        appConfig_.modbusBus.uart_parity =
            static_cast<app::modbus::bus::UART_Parity>(CONFIG_APP_MODBUS_UART_PARITY);
        appConfig_.modbusBus.uart_stop_bits =
            static_cast<app::modbus::bus::UART_Stop_Bits>(CONFIG_APP_MODBUS_UART_STOP_BITS);
    }

    void ConfigStore::State::initTl136Config()
    {
        appConfig_.tl136.ai_converter_address      = CONFIG_APP_MODBUS_TL136_ADDRESS;
        appConfig_.tl136.ai_converter_channel      = CONFIG_APP_MODBUS_TL136_INPUT_CHANNEL;
        appConfig_.tl136.ai_converter_calibration  = CONFIG_APP_MODBUS_TL136_CALIBRATION_FACTOR;
        appConfig_.tl136.probe_interval_ms         = CONFIG_APP_MODBUS_TL136_PROBE_INTERVAL_MS;
        appConfig_.tl136.full_scale_water_level_mm = CONFIG_APP_MODBUS_TL136_FULL_SCALE_LEVEL_MM;
    }
}
