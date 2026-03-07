#pragma once

#include "esp_event.h"

/**
 * @file i_wifi_event_handler.hpp
 * @brief Interface for handling system WiFi and IP events.
 */

namespace wifi_manager {

/**
 * @class IWiFiEventHandler
 * @brief Interface for handling system WiFi and IP events.
 * @internal
 */
class IWiFiEventHandler
{
public:
    virtual ~IWiFiEventHandler() = default;

    /**
     * @brief Handler for WiFi system events.
     * @internal
     * @param base Event base.
     * @param id Event ID.
     * @param data Event data.
     */
    virtual void handle_wifi_event(esp_event_base_t base, int32_t id, void *data) = 0;

    /**
     * @brief Handler for IP system events.
     * @internal
     * @param base Event base.
     * @param id Event ID.
     * @param data Event data.
     */
    virtual void handle_ip_event(esp_event_base_t base, int32_t id, void *data) = 0;
};

} // namespace wifi_manager
