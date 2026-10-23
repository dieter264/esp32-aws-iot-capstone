menu "1-Wire Bus"

    config APP_ONEWIRE_BUS_GPIO
        int "1-Wire GPIO number"
        range 0 SOC_GPIO_OUT_RANGE_MAX
        default 15
        help
            GPIO pin connected to the 1-Wire bus data line. The selected pin
            must support both input and output and must not be reserved by the
            board for flash, PSRAM, or another peripheral.

    config APP_ONEWIRE_BUS_MAX_RX_BYTES
        int "Maximum receive size in bytes"
        range 1 255
        default 10
        help
            Largest number of bytes that can be received in one transaction.
            The RMT backend allocates 32 bytes of internal memory for each byte
            configured here. Ten bytes is sufficient for typical 1-Wire ROM
            and DS18B20 transactions.

    config APP_ONEWIRE_BUS_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

endmenu
