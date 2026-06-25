#pragma once

#include "esp_log.h"
#include "esp_err.h"

#define LOGD_CALL(tag, err_var, func, func_args, post_fmt, ...)  \
    do                                                           \
    {                                                            \
        (err_var) = func func_args;                              \
        if ((err_var) == ESP_OK)                                 \
        {                                                        \
            ESP_LOGD((tag), "%s: %s: " post_fmt " -> ESP_OK",    \
                     __func__, #func, ##__VA_ARGS__);            \
        }                                                        \
        else                                                     \
        {                                                        \
            ESP_LOGE((tag), "%s: %s -> %s",                      \
                     __func__, #func, esp_err_to_name(err_var)); \
        }                                                        \
    } while (0)

#define LOGD_CALL_RET(tag, func, func_args, post_fmt, ...)                      \
    do                                                                          \
    {                                                                           \
        esp_err_t logd_err_var = ESP_FAIL;                                      \
        LOGD_CALL(tag, logd_err_var, func, func_args, post_fmt, ##__VA_ARGS__); \
        return logd_err_var;                                                    \
    } while (0)

#define LOGD_CALL_RET_ON_ERR(tag, func, func_args, post_fmt, ...)               \
    do                                                                          \
    {                                                                           \
        esp_err_t logd_err_var = ESP_FAIL;                                      \
        LOGD_CALL(tag, logd_err_var, func, func_args, post_fmt, ##__VA_ARGS__); \
        if (ESP_OK != logd_err_var)                                             \
        {                                                                       \
            return logd_err_var;                                                \
        }                                                                       \
    } while (0)

#define LOGE_ERR_RET(tag, err, fmt, ...)                                    \
    do                                                                      \
    {                                                                       \
        ESP_LOGE((tag), fmt ": %s", ##__VA_ARGS__, esp_err_to_name((err))); \
        return (err);                                                       \
    } while (0)
