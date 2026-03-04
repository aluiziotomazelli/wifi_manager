#include "wifi_bootstrapper.hpp"
#include "esp_log.h"
#include "wifi_event_handler.hpp"
// wifi_manager.hpp is intentionally NOT included here. The task entry point is
// injected via init(task_fn) so WiFiBootstrapper has no dependency on WiFiManager.

static const char *TAG = "WiFiBootstrapper";

namespace wifi_manager {

WiFiBootstrapper::WiFiBootstrapper(
    IWiFiDriverHAL &driver_hal,
    IWiFiConfigStorage &storage,
    IWiFiSyncManager &sync_manager)
    : driver_hal_(driver_hal)
    , storage_(storage)
    , sync_manager_(sync_manager)
    , event_handler_(&sync_manager_)
{
}

esp_err_t WiFiBootstrapper::init(TaskFunction_t task_fn, void *pvParameters, TaskHandle_t *pxTaskHandle)
{
    esp_err_t err = storage_.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Storage/NVS: %s", esp_err_to_name(err));
        deinit(pxTaskHandle);
        return err;
    }

    err = driver_hal_.netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_netif_init: %s", esp_err_to_name(err));
        deinit(pxTaskHandle);
        return err;
    }

    err = driver_hal_.event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        deinit(pxTaskHandle);
        return err;
    }

    sta_netif_ = driver_hal_.netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif_ == nullptr) {
        sta_netif_ = driver_hal_.netif_create_default_wifi_sta();
    }
    if (sta_netif_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        deinit(pxTaskHandle);
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = driver_hal_.wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to esp_wifi_init: %s", esp_err_to_name(err));
        deinit(pxTaskHandle);
        return err;
    }

    err = driver_hal_.wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        deinit(pxTaskHandle);
        return err;
    }

    err = sync_manager_.init();
    if (err != ESP_OK) {
        deinit(pxTaskHandle);
        return err;
    }

    err = driver_hal_.event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler::wifi_event_callback, &event_handler_, &wifi_event_instance_);
    if (err != ESP_OK) {
        deinit(pxTaskHandle);
        return err;
    }
    err = driver_hal_.event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler::ip_event_callback, &event_handler_, &ip_event_instance_);
    if (err != ESP_OK) {
        deinit(pxTaskHandle);
        return err;
    }

    storage_.ensure_config_fallback();

    // Use the injected task function pointer instead of a hardcoded symbol,
    // keeping WiFiBootstrapper decoupled from any concrete WiFiManager type.
    BaseType_t task_created = driver_hal_.task_create(task_fn, "wifi_task", 4096, pvParameters, 5, pxTaskHandle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi task");
        deinit(pxTaskHandle);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t WiFiBootstrapper::deinit(TaskHandle_t *pxTaskHandle)
{
    if (pxTaskHandle && *pxTaskHandle != nullptr) {
        ESP_LOGI(TAG, "Stopping WiFi task...");
        Message msg = {};
        msg.type = MessageType::COMMAND;
        msg.cmd = CommandId::EXIT;
        if (sync_manager_.is_initialized() && sync_manager_.post_message(msg) == ESP_OK) {
            int retry = 0;
            while (*pxTaskHandle != nullptr && retry < 100) {
                vTaskDelay(pdMS_TO_TICKS(10));
                retry++;
            }
        }

        if (*pxTaskHandle != nullptr) {
            ESP_LOGW(TAG, "WiFi task did not exit gracefully, deleting...");
            driver_hal_.task_delete(*pxTaskHandle);
            *pxTaskHandle = nullptr;
        }
    }

    esp_err_t ret = ESP_OK;
    esp_err_t err;

    if (wifi_event_instance_ != nullptr) {
        err = driver_hal_.event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance_);
        wifi_event_instance_ = nullptr;
        if (err != ESP_OK)
            ret = err;
    }
    if (ip_event_instance_ != nullptr) {
        err = driver_hal_.event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_instance_);
        ip_event_instance_ = nullptr;
        if (err != ESP_OK)
            ret = err;
    }

    driver_hal_.wifi_deinit();
    if (sta_netif_) {
        driver_hal_.netif_destroy_default_wifi(sta_netif_);
        sta_netif_ = nullptr;
    }
    sync_manager_.deinit();

    return ret;
}

} // namespace wifi_manager
