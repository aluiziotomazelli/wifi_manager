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
    virtual esp_err_t init_netif() = 0;
    virtual esp_err_t create_default_event_loop() = 0;
    virtual esp_err_t setup_sta_netif() = 0;
    virtual esp_err_t init_wifi() = 0;
    virtual esp_err_t set_mode_sta() = 0;

    // Event Handling
    virtual esp_err_t
    register_event_handlers(esp_event_handler_t wifi_handler, esp_event_handler_t ip_handler, void *handler_arg) = 0;
    virtual esp_err_t unregister_event_handlers() = 0;

    // Driver Operations
    virtual esp_err_t start() = 0;
    virtual esp_err_t stop() = 0;
    virtual esp_err_t connect() = 0;
    virtual esp_err_t disconnect() = 0;
    virtual esp_err_t restore() = 0;

    // Configuration
    virtual esp_err_t set_config(wifi_config_t *cfg) = 0;
    virtual esp_err_t get_config(wifi_config_t *cfg) = 0;

    // Cleanup
    virtual esp_err_t deinit() = 0;

    // Getters
    virtual esp_netif_t *get_sta_netif() const = 0;
};

} // namespace wifi_manager
