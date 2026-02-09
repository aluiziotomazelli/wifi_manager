#include "esp_wifi_types.h"
#include "unity.h"
#include "wifi_event_handler.hpp"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

using namespace wifi_manager;

TEST_CASE("WiFiEventHandler: Translator Test", "[event]")
{
    // Create a queue to receive messages
    QueueHandle_t queue = xQueueCreate(10, sizeof(Message));
    TEST_ASSERT_NOT_NULL(queue);

    // 1. Test WIFI_EVENT_STA_START -> EventId::STA_START
    WiFiEventHandler::wifi_event_handler(queue, WIFI_EVENT, WIFI_EVENT_STA_START, nullptr);

    Message msg;
    TEST_ASSERT_TRUE(xQueueReceive(queue, &msg, 0));
    TEST_ASSERT_EQUAL(MessageType::EVENT, msg.type);
    TEST_ASSERT_EQUAL(EventId::STA_START, msg.event);

    // 2. Test WIFI_EVENT_STA_CONNECTED -> EventId::STA_CONNECTED
    WiFiEventHandler::wifi_event_handler(queue, WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, nullptr);
    TEST_ASSERT_TRUE(xQueueReceive(queue, &msg, 0));
    TEST_ASSERT_EQUAL(EventId::STA_CONNECTED, msg.event);

    // 3. Test WIFI_EVENT_STA_DISCONNECTED -> EventId::STA_DISCONNECTED
    // Note: Disconnect event usually has data (reason), but handler might check it.
    // For unit test, we can pass nullptr if the handler handles it safely, or mock data.
    // Looking at implementation, it might cast data to wifi_event_sta_disconnected_t*.
    // Let's check implementation of wifi_event_handler.cpp in next step if this crashes.
    // For now, let's assume it checks for nullptr or we provide dummy data.
    wifi_event_sta_disconnected_t disc_data = {};
    disc_data.ssid_len                      = 0;
    disc_data.reason                        = WIFI_REASON_AUTH_EXPIRE;
    WiFiEventHandler::wifi_event_handler(nullptr, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disc_data);

    // Wait, the handler signature is (void* arg, esp_event_base_t base, int32_t id, void* data).
    // The 'arg' is usually the queue handle in our design?
    // Let's verify how it's registered in WiFiManager.cpp.
    // WiFiManager passes 'queue' as 'arg'.

    WiFiEventHandler::wifi_event_handler(queue, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disc_data);
    TEST_ASSERT_TRUE(xQueueReceive(queue, &msg, 0));
    TEST_ASSERT_EQUAL(EventId::STA_DISCONNECTED, msg.event);

    vQueueDelete(queue);
}
