#pragma once

#include "sdkconfig.h"

namespace app::cfg
{
    inline constexpr const char *FIRMWARE_VERSION    = "0.1";
    inline constexpr const char *BUILD_DATE          = __DATE__;
    inline constexpr const char *BUILD_TIME          = __TIME__;
    inline constexpr const char *IDF_TARGET          = CONFIG_IDF_TARGET;
    inline constexpr const char *ESP_IDF_VERSION_STR = IDF_VER;
}
