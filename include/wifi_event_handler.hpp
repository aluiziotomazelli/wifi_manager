#pragma once

#include "esp_event.h"
#include "wifi_types.hpp"

namespace wifi_manager {

/**
 * @class WiFiEventHandler
 * @brief Translates raw ESP-IDF events into WiFiManager EventId signals.
 */
class WiFiEventHandler
{
public:
    /**
     * @brief Static callback for WiFi system events.
     * @param arg Pointer to the command queue (QueueHandle_t).
     */
    static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    /**
     * @brief Static callback for IP system events.
     * @param arg Pointer to the command queue (QueueHandle_t).
     */
    static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
};

} // namespace wifi_manager
