#pragma once

#include "esp_event.h"
#include "wifi_types.hpp"
#include "interfaces/i_wifi_event_handler.hpp"

namespace wifi_manager {

class IWiFiSyncManager;

/**
 * @class WiFiEventHandler
 * @brief Translates raw ESP-IDF events into WiFiManager EventId signals.
 */
class WiFiEventHandler : public IWiFiEventHandler
{
public:
    explicit WiFiEventHandler(IWiFiSyncManager *sync_manager);
    ~WiFiEventHandler() override = default;

    /**
     * @brief Handler for WiFi system events.
     */
    void handle_wifi_event(esp_event_base_t base, int32_t id, void *data) override;

    /**
     * @brief Handler for IP system events.
     */
    void handle_ip_event(esp_event_base_t base, int32_t id, void *data) override;

    /**
     * @brief Static callback for WiFi system events (to be used with esp_event_loop).
     * @param arg Pointer to IWiFiEventHandler instance.
     */
    static void wifi_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data);

    /**
     * @brief Static callback for IP system events (to be used with esp_event_loop).
     * @param arg Pointer to IWiFiEventHandler instance.
     */
    static void ip_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data);

private:
    IWiFiSyncManager *sync_manager_;
};

} // namespace wifi_manager
