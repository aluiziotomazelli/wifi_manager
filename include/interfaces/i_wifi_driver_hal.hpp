#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

/**
 * @file i_wifi_driver_hal.hpp
 * @brief Interface for Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 */

namespace wifi_manager {

/**
 * @class IWiFiDriverHAL
 * @brief Interface for Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 * @internal
 */
class IWiFiDriverHAL
{
public:
    virtual ~IWiFiDriverHAL() = default;

    /**
     * @brief Initialize the TCP/IP stack.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t netif_init() = 0;

    /**
     * @brief Create the default event loop.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t event_loop_create_default() = 0;

    /**
     * @brief Create default network interface for WiFi Station.
     * @internal
     * @return Pointer to the netif instance.
     */
    virtual esp_netif_t *netif_create_default_wifi_sta() = 0;

    /**
     * @brief Get netif handle from interface key.
     * @internal
     * @param if_key Interface key.
     * @return Pointer to the netif instance.
     */
    virtual esp_netif_t *netif_get_handle_from_ifkey(const char *if_key) = 0;

    /**
     * @brief Initialize WiFi with the provided configuration.
     * @internal
     * @param cfg WiFi init configuration.
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_init(wifi_init_config_t *cfg) = 0;

    /**
     * @brief Set the WiFi operating mode.
     * @internal
     * @param mode WiFi mode.
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_set_mode(wifi_mode_t mode) = 0;

    /**
     * @brief Register an event handler instance.
     * @internal
     * @param event_base Event base.
     * @param event_id Event ID.
     * @param event_handler Event handler function.
     * @param handler_arg Argument for the handler.
     * @param instance[out] Output instance handle.
     * @return ESP_OK on success.
     */
    virtual esp_err_t event_handler_instance_register(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *handler_arg,
        esp_event_handler_instance_t *instance) = 0;

    /**
     * @brief Unregister an event handler instance.
     * @internal
     * @param event_base Event base.
     * @param event_id Event ID.
     * @param instance Instance handle to unregister.
     * @return ESP_OK on success.
     */
    virtual esp_err_t event_handler_instance_unregister(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t instance) = 0;

    /**
     * @brief Start the WiFi driver.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_start() = 0;

    /**
     * @brief Stop the WiFi driver.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_stop() = 0;

    /**
     * @brief Connect the WiFi driver.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_connect() = 0;

    /**
     * @brief Disconnect the WiFi driver.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_disconnect() = 0;

    /**
     * @brief Restore WiFi settings to factory defaults.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_restore() = 0;

    /**
     * @brief Set the WiFi configuration.
     * @internal
     * @param cfg Pointer to the configuration structure.
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_set_config(wifi_config_t *cfg) = 0;

    /**
     * @brief Get the current WiFi configuration.
     * @internal
     * @param cfg Pointer to the configuration structure.
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_get_config(wifi_config_t *cfg) = 0;

    /**
     * @brief Deinitialize the WiFi driver.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t wifi_deinit() = 0;

    /**
     * @brief Destroy the default WiFi netif.
     * @internal
     * @param netif Pointer to the netif instance.
     */
    virtual void netif_destroy_default_wifi(esp_netif_t *netif) = 0;

    /**
     * @brief Create a FreeRTOS task.
     * @internal
     * @param pvTaskCode Task entry function.
     * @param pcName Task name.
     * @param usStackDepth Stack depth.
     * @param pvParameters Task parameters.
     * @param uxPriority Task priority.
     * @param pxCreatedTask[out] Output task handle.
     * @return pdPASS on success.
     */
    virtual BaseType_t task_create(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t *const pxCreatedTask) = 0;

    /**
     * @brief Delete a FreeRTOS task.
     * @internal
     * @param xTaskToDelete Task handle.
     */
    virtual void task_delete(TaskHandle_t xTaskToDelete) = 0;
};

} // namespace wifi_manager
