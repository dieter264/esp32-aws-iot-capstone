#pragma once

#include "sdkconfig.h"

namespace app::cfg
{
    /** Semantic firmware version reported by the application. */
    inline constexpr const char *FIRMWARE_VERSION    = "0.4";
    /** Compiler-provided build date. */
    inline constexpr const char *BUILD_DATE          = __DATE__;
    /** Compiler-provided build time. */
    inline constexpr const char *BUILD_TIME          = __TIME__;
    /** ESP-IDF target selected for this build. */
    inline constexpr const char *IDF_TARGET          = CONFIG_IDF_TARGET;
    /** ESP-IDF version used to build the firmware. */
    inline constexpr const char *ESP_IDF_VERSION_STR = IDF_VER;
}
