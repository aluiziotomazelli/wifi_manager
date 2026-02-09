#include "unity.h"
#include "wifi_sync_manager.hpp"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host_test_common.hpp"

using namespace wifi_manager;

void setUp(void)
{
    host_test_setup_common_mocks();
}

void tearDown(void)
{
}

TEST_CASE("WiFiSyncManager: Initialization", "[sync]")
{
    WiFiSyncManager sync;
    TEST_ASSERT_EQUAL(ESP_OK, sync.init());
    TEST_ASSERT_TRUE(sync.is_initialized());
    sync.deinit();
    TEST_ASSERT_FALSE(sync.is_initialized());
}

TEST_CASE("WiFiSyncManager: Event Bits", "[sync]")
{
    WiFiSyncManager sync;
    sync.init();

    // Set bits
    sync.set_bits(wifi_manager::STARTED_BIT);

    // Wait for bits with timeout
    uint32_t bits = sync.wait_for_bits(wifi_manager::STARTED_BIT, 100);
    TEST_ASSERT_EQUAL(wifi_manager::STARTED_BIT, bits & wifi_manager::STARTED_BIT);

    // Clear bits
    sync.clear_bits(wifi_manager::STARTED_BIT);
    bits = sync.wait_for_bits(wifi_manager::STARTED_BIT, 10);
    TEST_ASSERT_EQUAL(0, bits & wifi_manager::STARTED_BIT);

    sync.deinit();
}

TEST_CASE("WiFiSyncManager: Message Queue", "[sync]")
{
    WiFiSyncManager sync;
    sync.init();

    Message msg_send = {};
    msg_send.type    = MessageType::COMMAND;
    msg_send.cmd     = CommandId::START;

    // Send message (async)
    TEST_ASSERT_EQUAL(ESP_OK, sync.post_message(msg_send));

    // Receive message manually from queue
    Message msg_recv = {};
    TEST_ASSERT_TRUE(xQueueReceive(sync.get_queue(), &msg_recv, pdMS_TO_TICKS(100)));

    TEST_ASSERT_EQUAL(MessageType::COMMAND, msg_recv.type);
    TEST_ASSERT_EQUAL(CommandId::START, msg_recv.cmd);

    sync.deinit();
}
