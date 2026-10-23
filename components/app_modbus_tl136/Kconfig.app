menu "TL136 water level sensor"

    comment "TL136: 4-20 mA, 0-2 m water; R4CVA02: 5 V, 0-20 mA"
    comment "UART settings are configured under Modbus Bus"

    config APP_MODBUS_TL136_ADDRESS
        int "Modbus device address"
        range 1 247
        default 1
        help
            Address of the R4CVA02 converter connected to the TL136.
            This selects the device to contact; changing this value
            does not reprogram the converter's address.

    config APP_MODBUS_TL136_INPUT_CHANNEL
        int "Converter input channel"
        range 1 2
        default 1
        help
            R4CVA02 input connected to the TL136:
            1 selects IN1, 2 selects IN2.

    config APP_MODBUS_TL136_CALIBRATION_FACTOR
        int "Converter calibration factor (1000 = unity)"
        range 1 65535
        default 1000
        help
            Calibration factor for the selected converter input.
            A value of 1000 leaves the reading unchanged;
            1010 increases it by 1 percent, and 990 decreases it
            by 1 percent.
            This corrects the converter's current measurement
            before conversion to water level.

    config APP_MODBUS_TL136_PROBE_INTERVAL_MS
        int "Water level probe interval (ms)"
        range 1000 3600000
        default 5000
        help
            Interval between water level measurements.
            The minimum interval follows the application's sensor
            polling policy.

    config APP_MODBUS_TL136_FULL_SCALE_LEVEL_MM
        int "Full-scale water level (mm)"
        range 1 65535
        default 2000
        help
            Water level corresponding to the sensor's 20 mA output.
            The 4 mA output corresponds to zero water level.
            Use 2000 for the installed 0-2 m sensor.
            This value describes the sensor's rating; it does not
            change its physical measuring range.

    config APP_MODBUS_TL136_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

endmenu