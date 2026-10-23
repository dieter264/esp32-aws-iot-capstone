#include "main_includes.hpp"

constexpr auto APP_LOG_GLOBAL_LEVEL = static_cast<esp_log_level_t>(CONFIG_APP_LOG_GLOBAL_LEVEL);

EventGroupHandle_t eventGroup                        = nullptr;
esp_event_handler_instance_t appEventHandlerInstance = nullptr;

static void appEventHandler(void *, esp_event_base_t, int32_t, void *);

void printVersion()
{
    ESP_LOGI(TAG, "Firmware %s built %s %s",
             app::cfg::FIRMWARE_VERSION,
             app::cfg::BUILD_DATE,
             app::cfg::BUILD_TIME);
}

void setAppLogging()
{
    esp_log_level_set("*", APP_LOG_GLOBAL_LEVEL);
    app::cfg::apply_log_level();
    app::dm::apply_log_level();
    app::https::apply_log_level();
    app::mqtt::apply_log_level();
    app::ota::apply_log_level();
    app::rm::apply_log_level();
    app::sm::apply_log_level();
    app::sntp::apply_log_level();
    app::wm::apply_log_level();

    esp_log_level_set("event", ESP_LOG_INFO);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);
    esp_log_level_set("httpd_sess", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_uri", ESP_LOG_WARN);
    esp_log_level_set("transport_base", ESP_LOG_WARN);
    esp_log_level_set("vfs_calls", ESP_LOG_WARN);
}

esp_err_t initEvents()
{
    eventGroup = xEventGroupCreate();

    ESP_RETURN_ON_FALSE(nullptr != eventGroup, ESP_ERR_NO_MEM,
                        TAG, "Could not create event group");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
                            app::sm::APP_EVENT, app::sm::APP_EVENT_RESTART_REQUEST,
                            &(appEventHandler), nullptr,
                            &(appEventHandlerInstance)),
                        TAG, "Could not register event handler");

    ESP_LOGD(TAG, "Event handling initialized");
    return ESP_OK;
}

void runSupervisorLoop()
{
    constexpr TickType_t WAIT_TIMEOUT_MS = pdMS_TO_TICKS(1000);

    for (;;)
    {
        const EventBits_t bits = xEventGroupWaitBits(
            eventGroup,
            TASK_WAIT_BIT_MASK,
            pdTRUE,
            pdFALSE,
            WAIT_TIMEOUT_MS);

        if (0 != (bits & TaskWakeBit::RESTART_REQUEST))
        {
            break;
        }

        monitorComponents();
    }
}

void finiEvents()
{
    if (nullptr != appEventHandlerInstance)
    {
        esp_event_handler_instance_unregister(app::sm::APP_EVENT,
                                              app::sm::APP_EVENT_RESTART_REQUEST,
                                              appEventHandlerInstance);
        appEventHandlerInstance = nullptr;
    }

    if (nullptr != eventGroup)
    {
        vEventGroupDelete(eventGroup);
        eventGroup = nullptr;
    }
}

extern "C" void app_main(void)
{
    printVersion();
    setAppLogging();
    ESP_ERROR_CHECK(createAndInitComponents());
    ESP_ERROR_CHECK(initEvents());
    runSupervisorLoop();
    finiEvents();
    finiAndDeleteComponents();
    esp_restart();
}

void appEventHandler(void *, esp_event_base_t eventBase, int32_t eventId, void *)
{
    ESP_LOGD(TAG, "Received event base= %s, id= %d", eventBase, eventId);

    switch (eventId)
    {
    case app::sm::APP_EVENT_RESTART_REQUEST:
    {
        requestRestart();
    }
    break;

    default:
        ESP_LOGD(TAG, "Unhandled event=%d", eventId);
    }
}

bool isRestartRequested()
{
    if (nullptr == eventGroup)
        return false;

    return 0 != (xEventGroupGetBits(eventGroup) & TaskWakeBit::RESTART_REQUEST);
}

void requestRestart()
{
    if (nullptr == eventGroup)
        return;

    xEventGroupSetBits(eventGroup, TaskWakeBit::RESTART_REQUEST);
}

void logTaskStackAndHeap()
{
    static size_t lastStackMinFreeBytes        = 0;
    static size_t lastHeapMinFreeBytes         = 0;
    static size_t lastHeapInternalMinFreeBytes = 0;

    const size_t stackMinFreeBytes        = uxTaskGetStackHighWaterMark(nullptr);
    const size_t heapMinFreeBytes         = esp_get_minimum_free_heap_size();
    const size_t heapInternalMinFreeBytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

    if ((lastStackMinFreeBytes != stackMinFreeBytes) ||
        (lastHeapMinFreeBytes != heapMinFreeBytes) ||
        (lastHeapInternalMinFreeBytes != heapInternalMinFreeBytes))
    {
        ESP_LOGI(TAG, "stack_min_free=%u, heap_min_free=%u, heap_int_min_free=%u bytes",
                 static_cast<unsigned>(stackMinFreeBytes),
                 static_cast<unsigned>(heapMinFreeBytes),
                 static_cast<unsigned>(heapInternalMinFreeBytes));

        lastStackMinFreeBytes        = stackMinFreeBytes;
        lastHeapMinFreeBytes         = heapMinFreeBytes;
        lastHeapInternalMinFreeBytes = heapInternalMinFreeBytes;
    }
}
