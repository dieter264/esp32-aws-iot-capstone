#include "private_include/bme280_internal.hpp"

namespace app::i2c::bme280
{
    constexpr auto BME280_LOG_LEVEL        = static_cast<esp_log_level_t>(CONFIG_APP_BME280_LOG_LEVEL);
    constexpr auto BME280_CHIP_ID          = static_cast<uint8_t>(0x60);
    constexpr auto BME280_CHIP_ID_REGISTER = static_cast<uint8_t>(0xD0);

    void apply_log_level()
    {
        if (-1 < BME280_LOG_LEVEL)
        {
            esp_log_level_set(TAG, BME280_LOG_LEVEL);
        }
    }

    static int16_t sign_extend_16(uint16_t x, uint8_t nBits)
    {
        uint16_t m = static_cast<uint16_t>(1u) << (nBits - 1);
        return static_cast<int16_t>((x ^ m) - m);
    }

    template <typename T>
    static T to16bit(const uint8_t *raw)
    {
        static_assert(std::is_integral_v<T>, "T must be integral");
        static_assert(sizeof(T) == 2, "T must be 16-bit");
        return static_cast<T>(
            (static_cast<uint16_t>(raw[1]) << 8) | static_cast<uint16_t>(raw[0]));
    }

    Bme280::Bme280(app::cfg::ConfigStore &store,
                   app::sm::StateMachine &sm,
                   app::i2c::bus::I2cBus &bus)
        : state_(std::make_unique<State>(store, sm, bus)) {}

    Bme280::~Bme280()
    {
        fini();
    }

    esp_err_t Bme280::init()
    {
        if (state_->isInitialized_) return ESP_OK;

        ESP_RETURN_ON_FALSE(state_->cfgStore_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "Config Store not initialized");

        ESP_RETURN_ON_FALSE(state_->stateMachine_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "State Machine not initialized");

        ESP_RETURN_ON_FALSE(state_->i2cBus_.isInitialized(), ESP_ERR_INVALID_STATE,
                            TAG, "I2C Bus not initialized");

        esp_err_t ret = ESP_FAIL;

        LOGD_CALL(TAG, ret, state_->initDevice, (), "Device initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initCalibration, (), "Device calibration initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initConfig, (), "Device config initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, state_->initTask, (), "Task initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        state_->isInitialized_ = true;
        ESP_LOGD(TAG, "BME280 initialized");
        return ESP_OK;
    }

    void Bme280::fini()
    {
        state_->finiTask();
        state_->finiDevice();

        if (!state_->isInitialized_) return;
        state_->isInitialized_ = false;
        ESP_LOGD(TAG, "BME280 finalized");
    }

    esp_err_t Bme280::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset BME280");

        ESP_LOGD(TAG, "BME280 reset");
        return ESP_OK;
    }

    bool Bme280::isInitialized() const
    {
        return state_->isInitialized_;
    }

    bool Bme280::hasConfigChanged() const
    {
        Bme280Config cfg{};
        state_->cfgStore_.getBme280Config(cfg);

        return (cfg != state_->initialCfg_);
    }

    esp_err_t Bme280::State::initDevice()
    {
        app::cfg::AppConfig appCfg;
        cfgStore_.getSnapshot(appCfg);

        const auto i2cBusCfg = appCfg.i2cBus;
        initialCfg_          = appCfg.bme280;
        auto portNum         = static_cast<i2c_port_num_t>(i2cBusCfg.i2c_port_num);

        i2c_master_bus_handle_t busHandle;
        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_get_bus_handle,
                             (portNum, &busHandle),
                             "busHandle= %p", busHandle);

        ESP_RETURN_ON_ERROR(
            i2cBus_.getDevAddressForChipID(BME280_CHIP_ID,
                                           BME280_CHIP_ID_REGISTER,
                                           appCfg.bme280.device_address),
            TAG, "Failed to get device address");

        i2c_device_config_t devConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = appCfg.bme280.device_address,
            .scl_speed_hz    = i2cBusCfg.scl_speed_hz,
            .scl_wait_us     = 0,
            .flags           = {.disable_ack_check = false}};

        LOGD_CALL_RET(TAG, i2c_master_bus_add_device,
                      (busHandle, &devConfig, &devHandle_),
                      "Device added: devAddress= 0x%02x, sclSpeed= %d, devHandle_= %p",
                      devConfig.device_address, devConfig.scl_speed_hz, devHandle_);
    }

    esp_err_t Bme280::State::initCalibration()
    {
        app::i2c::bus::I2cBusConfig i2cBusCfg{};
        cfgStore_.getI2cBusConfig(i2cBusCfg);

        uint8_t regCal1  = 0x88;
        uint8_t cal1[26] = {0};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit_receive,
                             (devHandle_, &regCal1, 1, cal1, sizeof(cal1), i2cBusCfg.xfer_timeout_ms),
                             "Cal data 1 found at 0x%02x", regCal1);

        uint8_t regCal2 = 0xE1;
        uint8_t cal2[7] = {0};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit_receive,
                             (devHandle_, &regCal2, 1, cal2, sizeof(cal2), i2cBusCfg.xfer_timeout_ms),
                             "Cal data 2 found at 0x%02x", regCal2);

        calib_.T1 = to16bit<uint16_t>(&cal1[0]);
        calib_.T2 = to16bit<int16_t>(&cal1[2]);
        calib_.T3 = to16bit<int16_t>(&cal1[4]);

        calib_.P1 = to16bit<uint16_t>(&cal1[6]);
        calib_.P2 = to16bit<int16_t>(&cal1[8]);
        calib_.P3 = to16bit<int16_t>(&cal1[10]);
        calib_.P4 = to16bit<int16_t>(&cal1[12]);
        calib_.P5 = to16bit<int16_t>(&cal1[14]);
        calib_.P6 = to16bit<int16_t>(&cal1[16]);
        calib_.P7 = to16bit<int16_t>(&cal1[18]);
        calib_.P8 = to16bit<int16_t>(&cal1[20]);
        calib_.P9 = to16bit<int16_t>(&cal1[22]);

        calib_.H1      = cal1[25];
        calib_.H2      = to16bit<int16_t>(&cal2[0]);
        calib_.H3      = cal2[2];
        uint16_t raw12 = (static_cast<uint16_t>(cal2[3]) << 4) |
                         (static_cast<uint16_t>(cal2[4]) & 0x0Fu);
        calib_.H4      = sign_extend_16(raw12, 12);
        raw12          = (static_cast<uint16_t>(cal2[5]) << 4) |
                         (static_cast<uint16_t>(cal2[4]) >> 4);
        calib_.H5      = sign_extend_16(raw12, 12);
        calib_.H6      = static_cast<int8_t>(cal2[6]);

        const auto &c = calib_;

        ESP_LOGD(TAG, "BME280 calibration:\n"
                      "  T1=%6ld  T2=%6ld  T3=%6ld\n"
                      "  P1=%6" PRId64 "  P2=%6" PRId64 "  P3=%6" PRId64 "\n"
                      "  P4=%6" PRId64 "  P5=%6" PRId64 "  P6=%6" PRId64 "\n"
                      "  P7=%6" PRId64 "  P8=%6" PRId64 "  P9=%6" PRId64 "\n"
                      "  H1=%6ld  H2=%6ld  H3=%6ld\n"
                      "  H4=%6ld  H5=%6ld  H6=%6ld",
                 (long)c.T1, (long)c.T2, (long)c.T3,
                 c.P1, c.P2, c.P3, c.P4, c.P5, c.P6, c.P7, c.P8, c.P9,
                 (long)c.H1, (long)c.H2, (long)c.H3, (long)c.H4, (long)c.H5, (long)c.H6);

        return ESP_OK;
    }

    esp_err_t Bme280::State::initConfig()
    {
        app::cfg::AppConfig appCfg;
        cfgStore_.getSnapshot(appCfg);
        auto i2cBusCfg = appCfg.i2cBus;
        auto bme280Cfg = appCfg.bme280;

        uint8_t reg_ctrl_meas[2] = {0xF4, 0};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit,
                             (devHandle_, reg_ctrl_meas, 2, i2cBusCfg.xfer_timeout_ms),
                             "Device set to sleep mode for config");

        auto t_sb             = static_cast<uint8_t>(bme280Cfg.t_sb);
        auto filter           = static_cast<uint8_t>(bme280Cfg.filter);
        uint8_t reg_config[2] = {0xF5, static_cast<uint8_t>(t_sb << 5 | filter << 2)};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit,
                             (devHandle_, reg_config, 2, i2cBusCfg.xfer_timeout_ms),
                             "reg_config 0x%02x written to addr 0x%02x",
                             reg_config[1], reg_config[0]);

        auto osrs_h             = static_cast<uint8_t>(bme280Cfg.osrs_h);
        uint8_t reg_ctrl_hum[2] = {0xF2, osrs_h};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit,
                             (devHandle_, reg_ctrl_hum, 2, i2cBusCfg.xfer_timeout_ms),
                             "reg_ctrl_hum 0x%02x written to addr 0x%02x",
                             reg_ctrl_hum[1], reg_ctrl_hum[0]);

        auto osrs_t      = static_cast<uint8_t>(bme280Cfg.osrs_t);
        auto osrs_p      = static_cast<uint8_t>(bme280Cfg.osrs_p);
        auto mode        = static_cast<uint8_t>(bme280Cfg.mode);
        reg_ctrl_meas[1] = static_cast<uint8_t>(osrs_t << 5 | osrs_p << 2 | mode);

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit,
                             (devHandle_, reg_ctrl_meas, 2, i2cBusCfg.xfer_timeout_ms),
                             "reg_ctrl_meas 0x%02x written to addr 0x%02x",
                             reg_ctrl_meas[1], reg_ctrl_meas[0]);

        uint8_t all_config_buf[5] = {0xF2, 0, 0, 0, 0};

        LOGD_CALL_RET_ON_ERR(TAG, i2c_master_transmit_receive,
                             (devHandle_, all_config_buf, 1, &all_config_buf[1], 4, i2cBusCfg.xfer_timeout_ms),
                             "All configuration registers read");

        uint8_t val_ctrl_hum  = all_config_buf[1] & 0x07u;
        uint8_t val_ctrl_meas = all_config_buf[3];
        uint8_t val_config    = all_config_buf[4] & 0xFCu;

        ESP_LOGD(TAG, "BME280 configuration:\n"
                      "  config    at 0x%02x: 0x%02x\n"
                      "  ctrl_hum  at 0x%02x: 0x%02x\n"
                      "  ctrl meas at 0x%02x: 0x%02x",
                 reg_config[0], val_config, reg_ctrl_hum[0], val_ctrl_hum,
                 reg_ctrl_meas[0], val_ctrl_meas);

        if ((reg_ctrl_hum[1] != val_ctrl_hum) || (reg_ctrl_meas[1] != val_ctrl_meas) ||
            (reg_config[1] != val_config))
        {
            ESP_LOGD(TAG, "Configuration missmatch");
            return ESP_ERR_INVALID_STATE;
        }

        return ESP_OK;
    }

    void Bme280::State::finiDevice()
    {
        if (nullptr != devHandle_)
        {
            i2c_master_bus_rm_device(devHandle_);
            devHandle_ = nullptr;
        }
    }
}
