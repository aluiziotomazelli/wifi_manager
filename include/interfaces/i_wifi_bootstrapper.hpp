#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace wifi_manager {

/**
 * @class IWiFiBootstrapper
 * @brief Interface for orchestrating the initialization and deinitialization of WiFi components.
 */
class IWiFiBootstrapper
{
public:
    virtual ~IWiFiBootstrapper() = default;

    /**
     * @brief Initialize all WiFi sub-components and start the background task.
     * @param pvParameters Pointer to the WiFiManager instance to be passed to the task.
     * @param pxTaskHandle Output pointer to the created task handle.
     * @return ESP_OK on success, or an error code on failure.
     */
    virtual esp_err_t init(void *pvParameters, TaskHandle_t *pxTaskHandle) = 0;

    /**
     * @brief Deinitialize all WiFi sub-components and stop the background task.
     * @param pxTaskHandle Pointer to the task handle to be stopped and cleared.
     * @return ESP_OK on success, or an error code on failure.
     */
    virtual esp_err_t deinit(TaskHandle_t *pxTaskHandle) = 0;
};

} // namespace wifi_manager
