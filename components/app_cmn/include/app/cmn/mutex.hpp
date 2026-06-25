#pragma once

#include <mutex>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace app::cmn
{
    class Mutex
    {
    public:
        Mutex()
            : handle_{xSemaphoreCreateMutex()}
        {
            configASSERT(nullptr != handle_);
        }

        ~Mutex()
        {
            if (nullptr != handle_)
            {
                vSemaphoreDelete(handle_);
                handle_ = nullptr;
            }
        }

        Mutex(const Mutex &)            = delete;
        Mutex &operator=(const Mutex &) = delete;

        Mutex(Mutex &&)            = delete;
        Mutex &operator=(Mutex &&) = delete;

        void lock()
        {
            configASSERT(nullptr != handle_);
            configASSERT(pdTRUE == xSemaphoreTake(handle_, portMAX_DELAY));
        }

        void unlock()
        {
            configASSERT(nullptr != handle_);
            configASSERT(pdTRUE == xSemaphoreGive(handle_));
        }

    private:
        SemaphoreHandle_t handle_{};
    };
}