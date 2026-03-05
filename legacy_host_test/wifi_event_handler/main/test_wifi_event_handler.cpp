#include "esp_wifi_types.h"
#include "unity.h"
#include "wifi_event_handler.hpp"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "host_test_common.hpp"

using namespace wifi_manager;

void setUp(void)
{
    host_test_setup_common_mocks();
}

void tearDown(void)
{
}

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
    wifi_event_sta_disconnected_t disc_data = {};
    disc_data.reason                        = WIFI_REASON_AUTH_EXPIRE;

    WiFiEventHandler::wifi_event_handler(queue, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disc_data);
    TEST_ASSERT_TRUE(xQueueReceive(queue, &msg, 0));
    TEST_ASSERT_EQUAL(EventId::STA_DISCONNECTED, msg.event);

    vQueueDelete(queue);
}
