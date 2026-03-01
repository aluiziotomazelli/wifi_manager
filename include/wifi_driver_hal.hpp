#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <string>

#include "interfaces/i_wifi_driver_hal.hpp"

namespace wifi_manager {

/**
 * @class WiFiDriverHAL
 * @brief Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 */
class WiFiDriverHAL : public IWiFiDriverHAL
{
public:
    WiFiDriverHAL();
    ~WiFiDriverHAL() override;

    // System Initialization

    esp_err_t netif_init() override { return esp_netif_init(); };

    esp_err_t event_loop_create_default() override { return esp_event_loop_create_default(); };

    esp_netif_t *netif_create_default_wifi_sta() override { return esp_netif_create_default_wifi_sta(); };

    esp_netif_t *netif_get_handle_from_ifkey(const char *if_key) override
    {
        return esp_netif_get_handle_from_ifkey(if_key);
    };

    esp_err_t wifi_init(wifi_init_config_t *cfg) override { return esp_wifi_init(cfg); };
    esp_err_t wifi_set_mode(wifi_mode_t mode) override { return esp_wifi_set_mode(mode); };

    // Event Handling
    esp_err_t event_handler_instance_register(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *handler_arg,
        esp_event_handler_instance_t *instance) override
    {
        return esp_event_handler_instance_register(event_base, event_id, event_handler, handler_arg, instance);
    };

    esp_err_t event_handler_instance_unregister(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t instance) override
    {
        return esp_event_handler_instance_unregister(event_base, event_id, instance);
    };

    // Driver Operations
    esp_err_t wifi_start() override { return esp_wifi_start(); };
    esp_err_t wifi_stop() override { return esp_wifi_stop(); };
    esp_err_t wifi_connect() override { return esp_wifi_connect(); };
    esp_err_t wifi_disconnect() override { return esp_wifi_disconnect(); };
    esp_err_t wifi_restore() override { return esp_wifi_restore(); };

    // Configuration
    esp_err_t wifi_set_config(wifi_config_t *cfg) override { return esp_wifi_set_config(WIFI_IF_STA, cfg); };
    esp_err_t wifi_get_config(wifi_config_t *cfg) override { return esp_wifi_get_config(WIFI_IF_STA, cfg); };

    // Cleanup
    esp_err_t wifi_deinit() override { return esp_wifi_deinit(); };
};

} // namespace wifi_manager
