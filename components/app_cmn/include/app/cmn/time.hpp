#pragma once

#include <cstdint>
#include <sys/time.h>

namespace app::cmn::time
{
    inline uint64_t getUtcTimeUs()
    {
        struct timeval tv{};
        if (0 != gettimeofday(&tv, nullptr)) return 0ULL;
        return static_cast<uint64_t>(tv.tv_sec) * 1000000ULL + static_cast<uint64_t>(tv.tv_usec);
    }
}
