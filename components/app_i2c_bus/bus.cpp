#include "esp_check.h"
#include "driver/i2c_master.h"
#include "app/cmn/log.hpp"
#include "app/cfg/config_store.hpp"
#include "app/i2c/bus.hpp"

namespace app::i2c::bus
{
    constexpr auto TAG               = "app_i2c_bus";
    constexpr auto I2C_BUS_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_I2C_BUS_LOG_LEVEL);
    constexpr auto SDA_IO_PIN        = static_cast<gpio_num_t>(CONFIG_APP_I2C_BUS_SDA);
    constexpr auto SCL_IO_PIN        = static_cast<gpio_num_t>(CONFIG_APP_I2C_BUS_SCL);

    void apply_log_level()
    {
        if (-1 < I2C_BUS_LOG_LEVEL)
        {
            esp_log_level_set(TAG, I2C_BUS_LOG_LEVEL);
        }
    }

    struct I2cBus::State
    {
        i2c_master_bus_handle_t busHandle = nullptr;
    };

    I2cBus::I2cBus(const app::cfg::DeviceID devID, const app::cfg::ConfigStore &store)
        : devID_(devID), cfgStore_(store), state_(std::make_unique<State>()) {}

    I2cBus::~I2cBus()
    {
        fini();
    }

    esp_err_t I2cBus::init()
    {
        if (isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        app::i2c::bus::I2cBusConfig i2cBusCfg{};
        cfgStore_.getI2cBusConfig(i2cBusCfg);

        i2c_master_bus_config_t mBusConfig = {
            .i2c_port          = static_cast<i2c_port_num_t>(i2cBusCfg.i2c_port_num),
            .sda_io_num        = SDA_IO_PIN,
            .scl_io_num        = SCL_IO_PIN,
            .clk_source        = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority     = 0,
            .trans_queue_depth = 0,
            .flags             = {
                .enable_internal_pullup = false,
                .allow_pd               = false}};

        esp_err_t ret = ESP_FAIL;

        LOGD_CALL(TAG, ret, i2c_new_master_bus,
                  (&mBusConfig, &(state_->busHandle)),
                  "New bus created: port= %d, sda= %d, scl= %d",
                  mBusConfig.i2c_port, mBusConfig.sda_io_num, mBusConfig.scl_io_num);

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        isInitialized_ = true;
        ESP_LOGD(TAG, "I2C bus initialized");
        return ret;
    }

    void I2cBus::fini()
    {
        if (nullptr != state_->busHandle)
        {
            i2c_del_master_bus(state_->busHandle);
            state_->busHandle = nullptr;
        }

        if (!isInitialized_) return;
        isInitialized_ = false;
        ESP_LOGD(TAG, "I2C bus finalized");
    }

    esp_err_t I2cBus::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset I2C bus");

        ESP_LOGD(TAG, "I2C bus reset");
        return ESP_OK;
    }

    bool I2cBus::isInitialized() const
    {
        return isInitialized_;
    }

    esp_err_t I2cBus::getDevAddressForChipID(uint8_t chipID, uint8_t regAddress,
                                             uint16_t &devAddress)
    {
        ESP_RETURN_ON_FALSE(isInitialized_, ESP_ERR_INVALID_STATE,
                            TAG, "I2C bus not initialized");

        app::i2c::bus::I2cBusConfig i2cBusCfg{};
        cfgStore_.getI2cBusConfig(i2cBusCfg);

        esp_err_t ret                     = ESP_FAIL;
        i2c_device_config_t devConf       = {};
        devConf.scl_speed_hz              = i2cBusCfg.scl_speed_hz;
        i2c_master_dev_handle_t devHandle = {};

        const uint16_t preferredAddr = devAddress;
        bool fullScanIfnotFound      = (0 != preferredAddr);
        esp_err_t lastProbeError     = ESP_OK;
        uint8_t lastProbeAddress     = 0;

        for (; 128 > devAddress && ESP_OK != ret;)
        {
            devConf.device_address = devAddress;

            LOGD_CALL(TAG, ret, i2c_master_bus_add_device,
                      (state_->busHandle, &devConf, &devHandle),
                      "devAddr=%02x", devAddress);

            if (ESP_OK != ret)
            {
                lastProbeError   = ret;
                lastProbeAddress = devAddress;
            }

            if (ESP_OK == ret)
            {
                uint8_t id = 0;

                ret = i2c_master_transmit_receive(devHandle, &regAddress, 1, &id,
                                                  1, i2cBusCfg.xfer_timeout_ms);

                if ((ESP_OK == ret) && (chipID == id))
                {
                    ESP_LOGI(TAG, "Chip ID 0x%02x found in register=0x%02x at devAddr=0x%02x",
                             id, regAddress, devAddress);
                }
                else
                {
                    lastProbeError   = ret;
                    lastProbeAddress = devAddress;
                    ret              = ESP_ERR_NOT_FOUND;
                }
            }

            if (nullptr != devHandle)
            {
                const esp_err_t rmRet = i2c_master_bus_rm_device(devHandle);
                if (ESP_OK != rmRet)
                {
                    ESP_LOGD(TAG, "Removing device (devAddr=%02x) failed: %s",
                             devAddress, esp_err_to_name(rmRet));
                }
            }

            if (ESP_OK != ret)
            {
                if (fullScanIfnotFound)
                {
                    fullScanIfnotFound = false;
                    devAddress         = 0;
                }
                else
                {
                    devAddress++;
                    if (preferredAddr == devAddress)
                    {
                        devAddress++;
                    }
                }
            }
        }

        if (ESP_OK != ret)
        {
            ESP_LOGD(TAG, "Chip ID 0x%02x not found; last probe addr=0x%02x, last probe result=%s",
                     chipID, lastProbeAddress, esp_err_to_name(lastProbeError));
        }

        return ret;
    }
}
