#pragma once

#include <mutex>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace app::cmn
{
    /** BasicLockable adapter owning a non-recursive FreeRTOS mutex. */
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

        /** Blocks indefinitely until the mutex is acquired. */
        void lock()
        {
            configASSERT(nullptr != handle_);
            [[maybe_unused]] auto ret = xSemaphoreTake(handle_, portMAX_DELAY);
            configASSERT(pdTRUE == ret);
        }

        /** Releases the mutex. The calling task must own it. */
        void unlock()
        {
            configASSERT(nullptr != handle_);
            [[maybe_unused]] auto ret = xSemaphoreGive(handle_);
            configASSERT(pdTRUE == ret);
        }

    private:
        SemaphoreHandle_t handle_{};
    };
}
