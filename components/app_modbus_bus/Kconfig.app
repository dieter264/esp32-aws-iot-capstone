menu "Modbus Bus"

    config APP_MODBUS_UART_PORT
        int "UART port number"
        range 0 2 if SOC_UART_HP_NUM > 2
        range 0 1 if SOC_UART_HP_NUM > 1
        range 0 0
        default 1
        help
            UART controller used for the Modbus RTU bus. Select a controller
            that is not used by the console or another component.

    config APP_MODBUS_UART_TX_GPIO
        int "UART TX GPIO number"
        range 0 SOC_GPIO_OUT_RANGE_MAX
        default 17
        help
            GPIO connected to the RS485 transceiver's data input (DI).
            Select an available output-capable GPIO for the board wiring.

    config APP_MODBUS_UART_RX_GPIO
        int "UART RX GPIO number"
        range 0 SOC_GPIO_IN_RANGE_MAX
        default 18
        help
            GPIO connected to the RS485 transceiver's receiver output (RO).
            Select an available input-capable GPIO for the board wiring.

    choice APP_MODBUS_UART_RS485_TRANSCEIVER_SELECTION
        prompt "RS485 transceiver"
        default APP_MODBUS_UART_RS485_TRANSCEIVER_MAX485
        help
            RS485 transceiver connected to the UART. Select MAX485 for
            external direction control through the UART RTS signal, or
            MAX13487E for automatic direction control.

        config APP_MODBUS_UART_RS485_TRANSCEIVER_MAX485
            bool "MAX485"

        config APP_MODBUS_UART_RS485_TRANSCEIVER_MAX13487E
            bool "MAX13487E"
    endchoice

    config APP_MODBUS_UART_RS485_TRANSCEIVER
        int
        default 1 if APP_MODBUS_UART_RS485_TRANSCEIVER_MAX13487E
        default 0
        help
            RS485 transceiver enum value: 0=MAX485, 1=MAX13487E.

    config APP_MODBUS_UART_RTS_GPIO
        int "RS485 direction GPIO number"
        depends on APP_MODBUS_UART_RS485_TRANSCEIVER_MAX485
        range 0 SOC_GPIO_OUT_RANGE_MAX
        default 16
        help
            GPIO connected to the RS485 transceiver's DE and /RE inputs.
            The UART driver uses RTS to switch between transmitting and
            receiving in half-duplex mode.

    config APP_MODBUS_UART_BAUD_RATE
        int "UART baud rate"
        range 1200 115200
        default 9600
        help
            Serial communication speed in bits per second. All devices on
            the Modbus bus must use the same baud rate.

    config APP_MODBUS_RESPONSE_TOUT_MS
        int "Response timeout (ms)"
        range 0 30000
        default 250
        help
            Maximum time to wait for a device response to a request.
            Zero uses the Modbus stack default FMB_MASTER_TIMEOUT_MS_RESPOND.
            Keep nonzero values below FMB_MASTER_MAX_API_BLOCKING_TIME_MS.
            Longer timeouts also increase the time spent probing absent devices.

    choice APP_MODBUS_UART_PARITY_SELECTION
        prompt "UART parity"
        default APP_MODBUS_UART_PARITY_NONE
        help
            Parity used with eight data bits. Select the setting used by
            the devices connected to the Modbus bus.

        config APP_MODBUS_UART_PARITY_NONE
            bool "None"

        config APP_MODBUS_UART_PARITY_EVEN
            bool "Even"

        config APP_MODBUS_UART_PARITY_ODD
            bool "Odd"
    endchoice

    config APP_MODBUS_UART_PARITY
        int
        default 2 if APP_MODBUS_UART_PARITY_EVEN
        default 3 if APP_MODBUS_UART_PARITY_ODD
        default 0
        help
            UART parity enum value: 0=none, 2=even, 3=odd.

    choice APP_MODBUS_UART_STOP_BITS_SELECTION
        prompt "UART stop bits"
        default APP_MODBUS_UART_STOP_BITS_ONE
        help
            Number of stop bits. Select the setting used by the devices
            connected to the Modbus bus.

        config APP_MODBUS_UART_STOP_BITS_ONE
            bool "One"

        config APP_MODBUS_UART_STOP_BITS_TWO
            bool "Two"
    endchoice

    config APP_MODBUS_UART_STOP_BITS
        int
        default 3 if APP_MODBUS_UART_STOP_BITS_TWO
        default 1
        help
            UART stop-bit enum value: 1=one stop bit, 3=two stop bits.

    config APP_MODBUS_BUS_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

endmenu
