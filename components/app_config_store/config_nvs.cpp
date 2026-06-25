#include "private_include/config_internal.hpp"

namespace app::cfg
{
    esp_err_t ConfigStore::State::readNvs()
    {
        NvsHandleGuard nhg;
        LOGD_CALL_RET_ON_ERR(TAG, nvs_open, (NVS_NAMESPACE, NVS_READWRITE, &nhg.handle),
                             "NVS namespace '%s' opened", NVS_NAMESPACE);

        nvs_type_t nvsType{};
        auto ret = nvs_find_key(nhg.handle, NVS_KEY, &nvsType);
        if (ESP_ERR_NVS_NOT_FOUND == ret)
        {
            ESP_LOGD(TAG, "No entry found in NVS for key=%s", NVS_KEY);
            return ESP_OK;
        }

        if (ESP_OK != ret)
        {
            LOGE_ERR_RET(TAG, ret, "Could not search NVS key=%s", NVS_KEY);
        }

        if (NVS_TYPE_BLOB != nvsType)
        {
            ESP_LOGW(TAG, "Invalid NVS type: key=%s, type=0x%02x", NVS_KEY, nvsType);
            LOGD_CALL(TAG, ret, eraseKey, (nhg), "Invalid entry erased: key=%s", NVS_KEY);
            return ESP_OK;
        }

        auto heapBlob = std::make_unique<PersistentCfgV1>();
        ESP_RETURN_ON_FALSE(nullptr != heapBlob, ESP_ERR_NO_MEM, TAG,
                            "No memory for PersistentCfgV1");
        auto &blob = *heapBlob;

        ret = loadAndValidate(nhg, blob);
        if (ESP_OK != ret)
        {
            ESP_LOGW(TAG, "Validation failed: key=%s, err=%s", NVS_KEY, esp_err_to_name(ret));
            LOGD_CALL(TAG, ret, eraseKey, (nhg), "Invalid entry erased: key=%s", NVS_KEY);
            return ESP_OK;
        }

        ESP_LOGD(TAG, "Valid entry found in NVS: key=%s, version=%u, size=%u",
                 NVS_KEY, blob.hdr.version, blob.hdr.totalSize);

        appConfig_.i2cBus     = blob.i2cBus;
        appConfig_.bme280     = blob.bme280;
        appConfig_.wifiMgrAP  = blob.wifiMgrAP;
        appConfig_.wifiMgrSta = blob.wifiMgrSta;
        appConfig_.httpsSrv   = blob.httpsSrv;
        appConfig_.sntpClt    = blob.sntpClt;
        cstrCopy(appConfig_.otaMgr.url, blob.otaMgr.url);
        appConfig_.otaMgr.server_cert = blob.otaMgr.server_cert;
        cstrCopy(appConfig_.mqttClt.uri, blob.mqttClt.uri);
        appConfig_.mqttClt.broker_cert = blob.mqttClt.broker_cert;

        return ESP_OK;
    }

    esp_err_t ConfigStore::State::updateNvs(const AppConfig &appCfg)
    {
        NvsHandleGuard nhg;
        LOGD_CALL_RET_ON_ERR(TAG, nvs_open, (NVS_NAMESPACE, NVS_READWRITE, &nhg.handle),
                             "NVS namespace '%s' opened", NVS_NAMESPACE);

        auto heapBlob = std::make_unique<PersistentCfgV1>();
        ESP_RETURN_ON_FALSE(nullptr != heapBlob, ESP_ERR_NO_MEM, TAG,
                            "No memory for PersistentCfgV1");
        auto &blob = *heapBlob;

        blob.i2cBus     = appCfg.i2cBus;
        blob.bme280     = appCfg.bme280;
        blob.wifiMgrAP  = appCfg.wifiMgrAP;
        blob.wifiMgrSta = appCfg.wifiMgrSta;
        blob.httpsSrv   = appCfg.httpsSrv;
        blob.sntpClt    = appCfg.sntpClt;
        cstrCopy(blob.otaMgr.url, appCfg.otaMgr.url);
        cstrCopy(blob.otaMgr.server_cert, appCfg.otaMgr.server_cert.c_str());
        cstrCopy(blob.mqttClt.uri, appCfg.mqttClt.uri);
        cstrCopy(blob.mqttClt.broker_cert, appCfg.mqttClt.broker_cert.c_str());

        blob.hdr.magic     = PERSISTENT_CFG_MAGIC;
        blob.hdr.version   = PersistentCfgV1::VERSION;
        blob.hdr.totalSize = sizeof(PersistentCfgV1);
        blob.hdr.crc32     = getCrc32(blob);

        ESP_RETURN_ON_ERROR(nvs_set_blob(nhg.handle, NVS_KEY, &blob, sizeof(blob)), TAG,
                            "Failed writing data to NVS: key=%s, size=%zu",
                            NVS_KEY, sizeof(blob));

        ESP_RETURN_ON_ERROR(nvs_commit(nhg.handle), TAG,
                            "Failed committing data at NVS: key=%s", NVS_KEY);

        ESP_LOGD(TAG, "Data saved to NVS: key=%s, size=%zu", NVS_KEY, sizeof(blob));
        return ESP_OK;
    }

    esp_err_t ConfigStore::State::eraseKey(NvsHandleGuard &nhg)
    {
        auto ret = nvs_erase_key(nhg.handle, NVS_KEY);
        return (ESP_OK != ret) ? ret : nvs_commit(nhg.handle);
    }

    esp_err_t ConfigStore::State::loadAndValidate(NvsHandleGuard &nhg, PersistentCfgV1 &cfgV1)
    {
        size_t blobSize{};

        LOGD_CALL_RET_ON_ERR(TAG, nvs_get_blob, (nhg.handle, NVS_KEY, nullptr, &blobSize),
                             "Blob found in NVS: key=%s, size=%zu", NVS_KEY, blobSize);

        ESP_RETURN_ON_FALSE(sizeof(PersistentCfgHdr) <= blobSize,
                            ESP_ERR_INVALID_SIZE, TAG, "Invalid blob size=%zu", blobSize);

        auto buf = std::make_unique<uint8_t[]>(blobSize);
        LOGD_CALL_RET_ON_ERR(TAG, nvs_get_blob, (nhg.handle, NVS_KEY, buf.get(), &blobSize),
                             "Blob loaded from NVS: key=%s, size=%zu", NVS_KEY, blobSize);

        PersistentCfgHdr cfgHdr{};
        memcpy(&cfgHdr, buf.get(), sizeof(cfgHdr));

        ESP_RETURN_ON_FALSE(PERSISTENT_CFG_MAGIC == cfgHdr.magic,
                            ESP_ERR_INVALID_RESPONSE, TAG, "Invalid magic");

        ESP_RETURN_ON_FALSE(PersistentCfgV1::VERSION == cfgHdr.version,
                            ESP_ERR_INVALID_VERSION, TAG,
                            "Unsupported version: found=%u, expected=%u",
                            cfgHdr.version, PersistentCfgV1::VERSION);

        ESP_RETURN_ON_FALSE(((sizeof(PersistentCfgV1) == cfgHdr.totalSize) &&
                             (blobSize == cfgHdr.totalSize)),
                            ESP_ERR_INVALID_SIZE, TAG,
                            "Blob size mismatch: nvs=%u, hdr=%u, expected=%zu",
                            blobSize, cfgHdr.totalSize, sizeof(PersistentCfgV1));

        memcpy(&cfgV1, buf.get(), sizeof(cfgV1));

        ESP_RETURN_ON_FALSE(getCrc32(cfgV1) == cfgV1.hdr.crc32,
                            ESP_ERR_INVALID_CRC, TAG, "Invalid CRC");

        return ESP_OK;
    }

    uint32_t ConfigStore::State::getCrc32(const PersistentCfgV1 &cfg)
    {
        const auto *p = reinterpret_cast<const uint8_t *>(&cfg) + sizeof(PersistentCfgHdr);
        return esp_crc32_le(0, p, sizeof(PersistentCfgV1) - sizeof(PersistentCfgHdr));
    }
}
