#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "app/cmn/log.hpp"
#include "app/rm/resource_manager.hpp"

namespace app::rm
{
    constexpr auto TAG          = "app_resource_manager";
    constexpr auto RM_LOG_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_RM_LOG_LEVEL);

    void apply_log_level()
    {
        if (-1 < RM_LOG_LEVEL)
        {
            esp_log_level_set(TAG, RM_LOG_LEVEL);
        }
    }

    ResourceManager::ResourceManager() {}

    ResourceManager::~ResourceManager()
    {
        fini();
    }

    esp_err_t ResourceManager::init()
    {
        if (isInitialized_) return ESP_OK;

        esp_err_t ret = ESP_FAIL;
        LOGD_CALL(TAG, ret, nvs_flash_init, (), "NVS flash initialized");
        if (ESP_ERR_NVS_NO_FREE_PAGES == ret || ESP_ERR_NVS_NEW_VERSION_FOUND == ret)
        {
            LOGD_CALL(TAG, ret, nvs_flash_erase, (), "NVS flash erased");
            LOGD_CALL(TAG, ret, nvs_flash_init, (), "NVS flash initialized");
        }

        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, esp_netif_init, (), "NetIf initialized");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        LOGD_CALL(TAG, ret, esp_event_loop_create_default, (), "Default event loop created");
        if (ESP_OK != ret)
        {
            fini();
            return ret;
        }

        isInitialized_ = true;
        ESP_LOGD(TAG, "Resource Manager initialized");
        return ESP_OK;
    }

    void ResourceManager::fini()
    {
        esp_event_loop_delete_default();
        esp_netif_deinit();
        nvs_flash_deinit();

        if (!isInitialized_) return;
        isInitialized_ = false;
        ESP_LOGD(TAG, "Resource Manager finalized");
    }

    esp_err_t ResourceManager::reset()
    {
        fini();
        ESP_RETURN_ON_ERROR(init(), TAG, "Failed to reset Resource Manager");

        ESP_LOGD(TAG, "Resource Manager reset");
        return ESP_OK;
    }

    bool ResourceManager::isInitialized() const
    {
        return isInitialized_;
    }
}