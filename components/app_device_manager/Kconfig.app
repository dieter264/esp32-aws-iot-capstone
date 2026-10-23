menu "Device Manager"

    config APP_DM_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

    config APP_DM_BME280_ENABLED
        bool "Enable BME280 sensor"
        default n
        help
            Enable a connected BME280 sensor.

    config APP_DM_DS18B20_ENABLED
        bool "Enable DS18B20 sensor"
        default n
        help
            Enable a connected DS18B20 sensor.

    config APP_DM_TL136_ENABLED
        bool "Enable TL136 sensor"
        default n
        help
            Enable a connected TL136 sensor.

endmenu
