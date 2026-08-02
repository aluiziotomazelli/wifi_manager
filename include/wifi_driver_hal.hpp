#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <string>

#include "interfaces/i_wifi_driver_hal.hpp"

/**
 * @file wifi_driver_hal.hpp
 * @brief Concrete implementation of IWiFiDriverHAL for ESP-IDF.
 */

namespace wifi_manager {

/**
 * @class WiFiDriverHAL
 * @brief Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 */
class WiFiDriverHAL : public IWiFiDriverHAL
{
public:
    WiFiDriverHAL() = default;
    ~WiFiDriverHAL() override = default;

    /**
     * @copydoc IWiFiDriverHAL::netif_init()
     */
    esp_err_t netif_init() override { return esp_netif_init(); };

    /**
     * @copydoc IWiFiDriverHAL::event_loop_create_default()
     */
    esp_err_t event_loop_create_default() override { return esp_event_loop_create_default(); };

    /**
     * @copydoc IWiFiDriverHAL::netif_create_default_wifi_sta()
     */
    esp_netif_t *netif_create_default_wifi_sta() override { return esp_netif_create_default_wifi_sta(); };

    /**
     * @copydoc IWiFiDriverHAL::netif_get_handle_from_ifkey()
     */
    esp_netif_t *netif_get_handle_from_ifkey(const char *if_key) override
    {
        return esp_netif_get_handle_from_ifkey(if_key);
    };

    /**
     * @copydoc IWiFiDriverHAL::wifi_init()
     */
    esp_err_t wifi_init(wifi_init_config_t *cfg) override { return esp_wifi_init(cfg); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_set_mode()
     */
    esp_err_t wifi_set_mode(wifi_mode_t mode) override { return esp_wifi_set_mode(mode); };

    /**
     * @copydoc IWiFiDriverHAL::event_handler_instance_register()
     */
    esp_err_t event_handler_instance_register(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *handler_arg,
        esp_event_handler_instance_t *instance) override
    {
        return esp_event_handler_instance_register(event_base, event_id, event_handler, handler_arg, instance);
    };

    /**
     * @copydoc IWiFiDriverHAL::event_handler_instance_unregister()
     */
    esp_err_t event_handler_instance_unregister(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t instance) override
    {
        return esp_event_handler_instance_unregister(event_base, event_id, instance);
    };

    /**
     * @copydoc IWiFiDriverHAL::wifi_start()
     */
    esp_err_t wifi_start() override { return esp_wifi_start(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_stop()
     */
    esp_err_t wifi_stop() override { return esp_wifi_stop(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_connect()
     */
    esp_err_t wifi_connect() override { return esp_wifi_connect(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_disconnect()
     */
    esp_err_t wifi_disconnect() override { return esp_wifi_disconnect(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_restore()
     */
    esp_err_t wifi_restore() override { return esp_wifi_restore(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_set_config()
     */
    esp_err_t wifi_set_config(wifi_config_t *cfg) override { return esp_wifi_set_config(WIFI_IF_STA, cfg); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_get_config()
     */
    esp_err_t wifi_get_config(wifi_config_t *cfg) override { return esp_wifi_get_config(WIFI_IF_STA, cfg); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_deinit()
     */
    esp_err_t wifi_deinit() override { return esp_wifi_deinit(); };

    /**
     * @copydoc IWiFiDriverHAL::wifi_sta_get_ap_info()
     */
    esp_err_t wifi_sta_get_ap_info(wifi_ap_record_t *info) override
    {
        return esp_wifi_sta_get_ap_info(info);
    };

    /**
     * @copydoc IWiFiDriverHAL::netif_destroy_default_wifi()
     */
    void netif_destroy_default_wifi(esp_netif_t *netif) override { esp_netif_destroy_default_wifi(netif); };

    /**
     * @copydoc IWiFiDriverHAL::task_create()
     */
    BaseType_t task_create(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t *const pxCreatedTask) override
    {
        return xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
    }

    /**
     * @copydoc IWiFiDriverHAL::task_delete()
     */
    void task_delete(TaskHandle_t xTaskToDelete) override { vTaskDelete(xTaskToDelete); }
};

} // namespace wifi_manager
