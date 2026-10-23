menu "DS18B20"

    config APP_ONEWIRE_DS18B20_ADDRESS
        hex "1-Wire device address"
        range 0 0xFFFFFFFFFFFFFFFF
        default 0x0
        help
            64-bit ROM address of the DS18B20 sensor for the 1-Wire bus.

    config APP_ONEWIRE_DS18B20_PROBE_INTERVAL_MS
        int "Temperature probe interval (ms)"
        range 1000 3600000
        default 5000
        help
            Interval in milliseconds between two temperature measurements.
            The minimum value is 1000 ms, which allows enough time for a
            DS18B20 temperature conversion at 12-bit resolution.

    config APP_ONEWIRE_DS18B20_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

endmenu
