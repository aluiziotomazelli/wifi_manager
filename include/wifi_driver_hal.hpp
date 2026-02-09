#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <string>

/**
 * @class WiFiDriverHAL
 * @brief Hardware Abstraction Layer for ESP-IDF WiFi and Netif APIs.
 *
 * This class centralizes all hardware-specific calls, facilitating
 * testing and protecting the core manager logic from SDK changes.
 */
class WiFiDriverHAL
{
public:
    WiFiDriverHAL();
    ~WiFiDriverHAL();

    // System Initialization
    esp_err_t init_netif();
    esp_err_t create_default_event_loop();
    esp_err_t setup_sta_netif();
    esp_err_t init_wifi();
    esp_err_t set_mode_sta();

    // Event Handling
    esp_err_t register_event_handlers(esp_event_handler_t wifi_handler,
                                      esp_event_handler_t ip_handler,
                                      void *handler_arg);
    esp_err_t unregister_event_handlers();

    // Driver Operations
    esp_err_t start();
    esp_err_t stop();
    esp_err_t connect();
    esp_err_t disconnect();
    esp_err_t restore();

    // Configuration
    esp_err_t set_config(wifi_config_t *cfg);
    esp_err_t get_config(wifi_config_t *cfg);

    // Cleanup
    esp_err_t deinit();

    // Getters
    esp_netif_t *get_sta_netif() const
    {
        return m_sta_netif;
    }

private:
    esp_netif_t *m_sta_netif;
    esp_event_handler_instance_t m_wifi_event_instance;
    esp_event_handler_instance_t m_ip_event_instance;
    bool m_wifi_init_done;
};
