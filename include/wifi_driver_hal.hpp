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
    esp_err_t init_netif() override;
    esp_err_t create_default_event_loop() override;
    esp_err_t setup_sta_netif() override;
    esp_err_t init_wifi() override;
    esp_err_t set_mode_sta() override;

    // Event Handling
    esp_err_t register_event_handlers(
        esp_event_handler_t wifi_handler,
        esp_event_handler_t ip_handler,
        void *handler_arg) override;
    esp_err_t unregister_event_handlers() override;

    // Driver Operations
    esp_err_t start() override;
    esp_err_t stop() override;
    esp_err_t connect() override;
    esp_err_t disconnect() override;
    esp_err_t restore() override;

    // Configuration
    esp_err_t set_config(wifi_config_t *cfg) override;
    esp_err_t get_config(wifi_config_t *cfg) override;

    // Cleanup
    esp_err_t deinit() override;

    // Getters
    esp_netif_t *get_sta_netif() const override
    {
        return sta_netif_;
    }

private:
    esp_netif_t *sta_netif_;
    esp_event_handler_instance_t wifi_event_instance_;
    esp_event_handler_instance_t ip_event_instance_;
    bool wifi_init_done_;
};

} // namespace wifi_manager
