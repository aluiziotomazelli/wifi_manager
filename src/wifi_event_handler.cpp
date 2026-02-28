#include "wifi_event_handler.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace wifi_manager {

void WiFiEventHandler::wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    QueueHandle_t queue = static_cast<QueueHandle_t>(arg);
    if (!queue)
        return;

    Message msg = {};
    msg.type    = MessageType::EVENT;

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
            msg.reason    = disconn->reason;
            msg.rssi      = disconn->rssi;
        }
        break;
    default:
        return; // Ignore unhandled events
    }

    xQueueSendFromISR(queue, &msg, nullptr);
}

void WiFiEventHandler::ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    QueueHandle_t queue = static_cast<QueueHandle_t>(arg);
    if (!queue)
        return;

    Message msg = {};
    msg.type    = MessageType::EVENT;

    if (id == IP_EVENT_STA_GOT_IP) {
        msg.event = EventId::GOT_IP;
    }
    else {
        return;
    }

    xQueueSendFromISR(queue, &msg, nullptr);
}

} // namespace wifi_manager
