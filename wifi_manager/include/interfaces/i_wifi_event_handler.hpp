#pragma once

#include "esp_event.h"

namespace wifi_manager {

/**
 * @class IWiFiEventHandler
 * @brief Interface for handling system WiFi and IP events.
 */
class IWiFiEventHandler
{
public:
    virtual ~IWiFiEventHandler() = default;

    /**
     * @brief Handler for WiFi system events.
     */
    virtual void handle_wifi_event(esp_event_base_t base, int32_t id, void *data) = 0;

    /**
     * @brief Handler for IP system events.
     */
    virtual void handle_ip_event(esp_event_base_t base, int32_t id, void *data) = 0;
};

} // namespace wifi_manager
