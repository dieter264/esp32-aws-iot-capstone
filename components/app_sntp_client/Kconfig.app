menu "SNTP Client"

    config APP_SNTP_LOG_LEVEL
        int "Log level override"
        range -1 5
        default -1
        help
            -1=use global, 0=NONE..5=VERBOSE

    config APP_SNTP_ENABLED
        bool "Enable SNTP time synchronization"
        default n
        help
            Enable wall-clock time synchronization using SNTP/NTP.

    if APP_SNTP_ENABLED

        config APP_SNTP_SERVER_FROM_DHCP
            bool "Use NTP server provided by DHCP"
            default n
            help
                Use NTP/SNTP server addresses provided by the DHCP server.

        config APP_SNTP_STATIC_SERVER_1
            string "NTP server 1"
            default ""
            help
                Static NTP server.
                Optional, if DHCP-provided NTP is enabled. In this case the server is
                just used as fallback server. If DHCP-provided NTP is not enabled, this
                server is used for the NTP service.
                Static servers must be configured as a compact list: server 1 must be
                set before server 2 is used.
                Must be 0..63 chars.

        config APP_SNTP_STATIC_SERVER_2
            string "NTP server 2"
            default ""
            help
                Optional second static NTP server.
                Only used when server 1 is also configured; leave this empty unless
                APP_SNTP_STATIC_SERVER_1 is set.
                Must be 0..63 chars.

    endif

endmenu
