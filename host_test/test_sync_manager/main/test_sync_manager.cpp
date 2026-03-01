#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_sync_manager.hpp"

// NOTE: branches for allocation failure (xQueueCreate/xEventGroupCreate returning null)
// and full queue (xQueueSend returning pdFALSE) are not covered because they require
// FreeRTOS mock injection. Not worth the refactor at this stage.

using namespace wifi_manager;
using namespace testing;

TEST(WiFiSyncManagerTest, Initialization)
{
    WiFiSyncManager sync_manager;
    EXPECT_EQ(ESP_OK, sync_manager.init());
    EXPECT_TRUE(sync_manager.is_initialized());
    sync_manager.deinit();
    EXPECT_FALSE(sync_manager.is_initialized());
}

TEST(WiFiSyncManagerTest, EventBits)
{
    WiFiSyncManager sync_manager;
    sync_manager.init();

    // Set bits
    sync_manager.set_bits(wifi_manager::STARTED_BIT);

    // Wait for bits with timeout
    uint32_t bits = sync_manager.wait_for_bits(wifi_manager::STARTED_BIT, 10);
    EXPECT_EQ(wifi_manager::STARTED_BIT, bits & wifi_manager::STARTED_BIT);

    // Clear bits
    sync_manager.clear_bits(wifi_manager::STARTED_BIT);
    bits = sync_manager.wait_for_bits(wifi_manager::STARTED_BIT, 10);
    EXPECT_EQ(0, bits & wifi_manager::STARTED_BIT);

    sync_manager.deinit();
}

TEST(WiFiSyncManagerTest, PostMessage)
{
    WiFiSyncManager sync_manager;
    sync_manager.init();

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;

    // Send message
    EXPECT_EQ(ESP_OK, sync_manager.post_message(msg));

    // Receive message manually from queue
    wifi_manager::Message received_msg = {};

    EXPECT_TRUE(xQueueReceive(sync_manager.get_queue(), &received_msg, pdMS_TO_TICKS(100)));
    EXPECT_EQ(msg.type, received_msg.type);
    EXPECT_EQ(msg.cmd, received_msg.cmd);

    sync_manager.deinit();
}

TEST(WiFiSyncManagerTest, PostMessageFromIsr)
{
    WiFiSyncManager sync_manager;
    sync_manager.init();

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_START;

    EXPECT_EQ(ESP_OK, sync_manager.post_message_from_isr(msg));

    wifi_manager::Message received = {};
    EXPECT_TRUE(xQueueReceive(sync_manager.get_queue(), &received, pdMS_TO_TICKS(100)));
    EXPECT_EQ(msg.event, received.event);

    sync_manager.deinit();
}

TEST(WiFiSyncManagerTest, PostMessageBeforeInitReturnsInvalidState)
{
    WiFiSyncManager sync_manager;
    wifi_manager::Message msg = {};
    EXPECT_EQ(ESP_ERR_INVALID_STATE, sync_manager.post_message(msg));
    EXPECT_EQ(ESP_ERR_INVALID_STATE, sync_manager.post_message_from_isr(msg));
}

TEST(WiFiSyncManagerTest, EventBitsBeforeInitDoNotCrash)
{
    WiFiSyncManager sync_manager;
    EXPECT_NO_FATAL_FAILURE(sync_manager.set_bits(STARTED_BIT));
    EXPECT_NO_FATAL_FAILURE(sync_manager.clear_bits(STARTED_BIT));
    EXPECT_EQ(0, sync_manager.wait_for_bits(STARTED_BIT, 10));
}