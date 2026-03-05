#include "wifi_event_handler.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "interfaces/i_wifi_sync_manager.hpp"

namespace wifi_manager {

WiFiEventHandler::WiFiEventHandler(IWiFiSyncManager *sync_manager)
    : sync_manager_(sync_manager)
{
}

void WiFiEventHandler::handle_wifi_event(esp_event_base_t base, int32_t id, void *data)
{
    if (!sync_manager_)
        return;

    Message msg = {};
    msg.type = MessageType::EVENT;

    switch (id) {
    case WIFI_EVENT_STA_START:
        msg.event = EventId::STA_START;
        break;
    case WIFI_EVENT_STA_STOP:
        msg.event = EventId::STA_STOP;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        msg.event = EventId::STA_CONNECTED;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        msg.event = EventId::STA_DISCONNECTED;
        if (data != nullptr) {
            auto *disconn = static_cast<wifi_event_sta_disconnected_t *>(data);
            msg.reason = disconn->reason;
            msg.rssi = disconn->rssi;
        }
        break;
    default:
        return; // Ignore unhandled events
    }

    sync_manager_->post_message_from_isr(msg);
}

void WiFiEventHandler::handle_ip_event(esp_event_base_t base, int32_t id, void *data)
{
    if (!sync_manager_)
        return;

    Message msg = {};
    msg.type = MessageType::EVENT;

    if (id == IP_EVENT_STA_GOT_IP) {
        msg.event = EventId::GOT_IP;
    }
    else {
        return;
    }

    sync_manager_->post_message_from_isr(msg);
}

void WiFiEventHandler::wifi_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    IWiFiEventHandler *handler = static_cast<IWiFiEventHandler *>(arg);
    if (handler) {
        handler->handle_wifi_event(base, id, data);
    }
}

void WiFiEventHandler::ip_event_callback(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    IWiFiEventHandler *handler = static_cast<IWiFiEventHandler *>(arg);
    if (handler) {
        handler->handle_ip_event(base, id, data);
    }
}

} // namespace wifi_manager
