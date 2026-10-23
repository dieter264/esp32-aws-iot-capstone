#pragma once

#include <memory>
#include "esp_err.h"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::modbus::bus
{
    class ModbusBus;
}

namespace app::modbus::tl136
{
    class R4cva02
    {
    public:
        explicit R4cva02(app::cfg::ConfigStore &,
                         app::modbus::bus::ModbusBus &);

        ~R4cva02();

        R4cva02(const R4cva02 &)            = delete;
        R4cva02 &operator=(const R4cva02 &) = delete;

        esp_err_t init();

        void fini();

        esp_err_t reset();

        bool isInitialized() const;

        esp_err_t getCurrent_mAx100(uint16_t &);

    private:
        struct State;
        std::unique_ptr<State> state_{};
    };
}
