#pragma once

#include "esp_event.h"
#include "wifi_types.hpp"
#include "interfaces/i_wifi_event_handler.hpp"

/**
 * @file wifi_event_handler.hpp
 * @brief Concrete implementation of IWiFiEventHandler.
 */

namespace wifi_manager {

class IWiFiSyncManager;

/**
 * @class WiFiEventHandler
 * @brief Translates raw ESP-IDF events into WiFiManager EventId signals.
 */
class WiFiEventHandler : public IWiFiEventHandler
{
public:
    /**
     * @brief Construct a new WiFiEventHandler.
     *
     * @param sync_manager Pointer to the synchronization manager to post messages.
     */
    explicit WiFiEventHandler(IWiFiSyncManager *sync_manager);

    ~WiFiEventHandler() override = default;

    /**
     * @copydoc IWiFiEventHandler::handle_wifi_event()
     */
    void handle_wifi_event(esp_event_base_t base, int32_t id, void *data) override;

    /**
     * @copydoc IWiFiEventHandler::handle_ip_event()
     */
    void handle_ip_event(esp_event_base_t base, int32_t id, void *data) override;

    /**
     * @brief Static callback for WiFi system events (to be used with esp_event_loop).
     * @param arg Pointer to IWiFiEventHandler instance.
     * @param base Event base.
     * @param id Event ID.
     * @param data Event data.
     */
    static void wifi_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data);

    /**
     * @brief Static callback for IP system events (to be used with esp_event_loop).
     * @param arg Pointer to IWiFiEventHandler instance.
     * @param base Event base.
     * @param id Event ID.
     * @param data Event data.
     */
    static void ip_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data);

private:
    IWiFiSyncManager *sync_manager_; ///< Pointer to the synchronization manager
};

} // namespace wifi_manager
