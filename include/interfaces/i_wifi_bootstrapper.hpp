#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file i_wifi_bootstrapper.hpp
 * @brief Interface for orchestrating the initialization and deinitialization of WiFi components.
 */

namespace wifi_manager {

/**
 * @class IWiFiBootstrapper
 * @brief Interface for orchestrating the initialization and deinitialization of WiFi components.
 * @internal
 */
class IWiFiBootstrapper
{
public:
    virtual ~IWiFiBootstrapper() = default;

    /**
     * @brief Initialize all WiFi sub-components and start the background task.
     * @internal
     *
     * The task function is injected as a parameter so that WiFiBootstrapper does not depend
     * on any concrete WiFiManager symbol. The caller (WiFiManager) is responsible for passing
     * its own static task entry point (e.g. WiFiManager::wifi_task).
     *
     * @param task_fn         FreeRTOS-compatible task entry point (void(*)(void*)).
     * @param pvParameters     Opaque pointer forwarded to the task on creation (typically `this`).
     * @param pxTaskHandle     Output pointer to the created task handle.
     * @param task_stack_size  Stack size for the created task in bytes (default: 4096).
     * @param task_priority    Priority for the created task (default: 5).
     *
     * @return
     *     - ESP_OK: Success.
     *     - ESP_ERR_NO_MEM: Failed to create the background task or other resources.
     *     - Other: Error codes from sub-component initialization.
     */
    virtual esp_err_t init(
        TaskFunction_t task_fn,
        void *pvParameters,
        TaskHandle_t *pxTaskHandle,
        uint32_t task_stack_size = 4096,
        UBaseType_t task_priority = 5) = 0;

    /**
     * @brief Deinitialize all WiFi sub-components and stop the background task.
     * @internal
     *
     * @param pxTaskHandle Pointer to the task handle to be stopped and cleared.
     *
     * @return
     *     - ESP_OK: Success.
     *     - Other: Error codes from sub-component deinitialization.
     */
    virtual esp_err_t deinit(TaskHandle_t *pxTaskHandle) = 0;
};

} // namespace wifi_manager
