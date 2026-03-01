#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

namespace wifi_manager {

/**
 * @class IWiFiDriverHAL
 * @brief Interface for Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 */
class IWiFiDriverHAL
{
public:
    virtual ~IWiFiDriverHAL() = default;

    // System Initialization
    virtual esp_err_t netif_init() = 0;
    virtual esp_err_t event_loop_create_default() = 0;
    virtual esp_netif_t *netif_create_default_wifi_sta() = 0;
    virtual esp_netif_t *netif_get_handle_from_ifkey(const char *if_key) = 0;

    virtual esp_err_t wifi_init(wifi_init_config_t *cfg) = 0;
    virtual esp_err_t wifi_set_mode(wifi_mode_t mode) = 0;

    // Event Handling
    virtual esp_err_t event_handler_instance_register(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *handler_arg,
        esp_event_handler_instance_t *instance) = 0;
    virtual esp_err_t event_handler_instance_unregister(
        esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t instance) = 0;

    // Driver Operations
    virtual esp_err_t wifi_start() = 0;
    virtual esp_err_t wifi_stop() = 0;
    virtual esp_err_t wifi_connect() = 0;
    virtual esp_err_t wifi_disconnect() = 0;
    virtual esp_err_t wifi_restore() = 0;

    // Configuration
    virtual esp_err_t wifi_set_config(wifi_config_t *cfg) = 0;
    virtual esp_err_t wifi_get_config(wifi_config_t *cfg) = 0;

    // Cleanup
    virtual esp_err_t wifi_deinit() = 0;
    virtual esp_err_t netif_destroy_default_wifi(esp_netif_t *netif) = 0;
};

} // namespace wifi_manager
