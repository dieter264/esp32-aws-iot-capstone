#pragma once

#include "esp_err.h"

namespace app::rm
{
    /** Applies this component's configured ESP-IDF log level. */
    void apply_log_level();

    /** Owns process-wide NVS, network-interface, and default event-loop resources. */
    class ResourceManager
    {
    public:
        ResourceManager();

        ~ResourceManager();

        ResourceManager(const ResourceManager &) = delete;
        ResourceManager &operator=(const ResourceManager &) = delete;

        /** Initializes global ESP-IDF resources. Idempotent after success. */
        esp_err_t init();

        /** Releases the global resources; dependent components must already be stopped. */
        void fini();

        /** Releases and recreates all global resources. */
        esp_err_t reset();

        /** Returns whether all global resources are initialized. */
        bool isInitialized() const;

    private:
        bool isInitialized_ = false;
    };
}
