#include "app/modbus/mb_bus.hpp"

#include <mutex>

#include "freertos/FreeRTOS.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_modbus_master.h"
#include "sdkconfig.h"

#include "app/cfg/config_store.hpp"
#include "app/cfg/types.hpp"
#include "app/cmn/log.hpp"
#include "app/cmn/mutex.hpp"

namespace app::modbus::bus
{
    constexpr auto TAG              = "app_modbus_bus";
    constexpr auto MD_BUS_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_MODBUS_BUS_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < MD_BUS_LOG_LEVEL)
        {
            esp_log_level_set(TAG, MD_BUS_LOG_LEVEL);
        }
    }

    struct ModbusBus::State
    {
        explicit State(const app::cfg::ConfigStore &cfgStore)
            : cfgStore_(cfgStore) {}

        bool isInitialized_ = false;
        app::cmn::Mutex mutex_{};

        const app::cfg::ConfigStore &cfgStore_;
        ModbusBusConfig initialCfg_{};

        mb_communication_info_t mbConfig_{};
        void *mbCtx_{};

        // Used to satisfy ESP-Modbus startup validation;
        // However, we will use raw requests.
        mb_parameter_descriptor_t dummyDescriptor_ = []
        {
            mb_parameter_descriptor_t descriptor{};
            descriptor.cid       = 0;
            descriptor.param_key = "unused";
            descriptor.mb_size   = 1;
            return descriptor;
        }();
    };

    ModbusBus::ModbusBus(const app::cfg::ConfigStore &store)
        : state_(std::make_unique<State>(store)) {}

    ModbusBus::~ModbusBus()
    {
        fini();
    }

    esp_err_t ModbusBus::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        state_->cfgStore_.getModbusBusConfig(state_->initialCfg_);

        esp_err_t ret         = ESP_FAIL;
        uart_port_t uart_port = static_cast<uart_port_t>(state_->initialCfg_.uart_port);

        state_->mbConfig_.ser_opts = [cfg = state_->initialCfg_, uart_port]
        {
            mb_serial_opts_t opts{};
            opts.mode             = MB_RTU;
            opts.port             = uart_port;
            opts.response_tout_ms = cfg.response_tout_ms;
            opts.baudrate         = cfg.uart_baud_rate;
            opts.data_bits        = UART_DATA_8_BITS;
            opts.stop_bits        = static_cast<uart_stop_bits_t>(cfg.uart_stop_bits);
            opts.parity           = static_cast<uart_parity_t>(cfg.uart_parity);
            return opts;
        }();

        LOGD_CALL(TAG, ret, mbc_master_create_serial,
                  (&state_->mbConfig_,
                   &state_->mbCtx_),
                  "Modbus controller created");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        auto uartMode     = UART_MODE_UART;
        auto uartModeName = "Regular";
        auto uartRtsGpio  = UART_PIN_NO_CHANGE;

        if (MAX485 == state_->initialCfg_.uart_rs485_transceiver)
        {
            uartMode     = UART_MODE_RS485_HALF_DUPLEX;
            uartModeName = "RS485 Half Duplex";
            uartRtsGpio  = state_->initialCfg_.uart_rts_gpio.value();
        }

        LOGD_CALL(TAG, ret, uart_set_pin,
                  (uart_port,
                   state_->initialCfg_.uart_tx_gpio,
                   state_->initialCfg_.uart_rx_gpio,
                   uartRtsGpio,
                   UART_PIN_NO_CHANGE),
                  "Set UART pin numbers: port=%u, tx=%u, rx=%u, rts=%d",
                  uart_port,
                  state_->initialCfg_.uart_tx_gpio,
                  state_->initialCfg_.uart_rx_gpio,
                  uartRtsGpio);

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, uart_set_mode,
                  (uart_port, uartMode),
                  "Set UART mode to %s", uartModeName);

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, mbc_master_set_descriptor,
                  (state_->mbCtx_,
                   &state_->dummyDescriptor_,
                   1),
                  "Set dummy descriptor");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, mbc_master_start,
                  (state_->mbCtx_),
                  "Modbus controller started");

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "Modbus bus initialized");
        return ret;
    }

    void ModbusBus::fini()
    {
        if (nullptr != state_->mbCtx_)
        {
            mbc_master_delete(state_->mbCtx_);
            state_->mbCtx_ = {};
        }

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "Modbus bus finalized");
    }

    esp_err_t ModbusBus::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset Modbus bus");

        ESP_LOGD(TAG, "Modbus bus reset");
        return ESP_OK;
    }

    bool ModbusBus::isInitialized() const
    {
        return state_->isInitialized_;
    }

    bool ModbusBus::hasConfigChanged() const
    {
        ModbusBusConfig cfg{};
        state_->cfgStore_.getModbusBusConfig(cfg);

        return (cfg != state_->initialCfg_);
    }

    esp_err_t ModbusBus::readHoldingRegister(uint8_t deviceAddr,
                                             uint16_t registerAddr,
                                             uint16_t &data)
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard g(state_->mutex_);

        mb_param_request_t request{
            .slave_addr           = deviceAddr,
            .command              = 0x03,
            .reg_start            = registerAddr,
            .reg_size             = 1,
            .wr_rd_multi_reg_func = {}};

        return mbc_master_send_request(state_->mbCtx_, &request, &data);
    }

    esp_err_t ModbusBus::writeHoldingRegister(uint8_t deviceAddr,
                                              uint16_t registerAddr,
                                              uint16_t data)
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard g(state_->mutex_);

        mb_param_request_t request{
            .slave_addr           = deviceAddr,
            .command              = 0x06,
            .reg_start            = registerAddr,
            .reg_size             = 1,
            .wr_rd_multi_reg_func = {}};

        return mbc_master_send_request(state_->mbCtx_, &request, &data);
    }

    esp_err_t ModbusBus::scanDeviceAddress(uint8_t deviceAddr)
    {
        configASSERT(state_->isInitialized_);
        std::lock_guard g(state_->mutex_);

        mb_param_request_t request{
            .slave_addr           = deviceAddr,
            .command              = 0x03,
            .reg_start            = 0x0000,
            .reg_size             = 1,
            .wr_rd_multi_reg_func = {}};

        uint16_t data{};
        auto ret = mbc_master_send_request(state_->mbCtx_, &request, &data);
        ESP_LOGD(TAG, "Checked device address=%u: return code=%s, data=0x%02" PRIX16,
                 deviceAddr, esp_err_to_name(ret), data);

        if (ESP_OK == ret || ESP_ERR_NOT_SUPPORTED == ret || ESP_ERR_INVALID_RESPONSE == ret)
        {
            return ESP_OK;
        }

        return ret;
    }
}
