menu "BME280"

    config APP_BME280_PROBE_INTERVAL_MS
        int "Sample probe interval (ms)"
        range 1000 3600000
        default 5000
        help
            Interval in milliseconds between two BME280 samples.
            The minimum value of 1000 ms limits sensor reads and MQTT
            publication frequency.

    config APP_BME280_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

endmenu
