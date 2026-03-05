#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_message_processor.hpp"

#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"

using namespace wifi_manager;
using namespace testing;

class MessageProcessorTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> driver_hal;
    NiceMock<MockWiFiConfigStorage> storage;
    NiceMock<MockWiFiSyncManager> sync_manager;
    NiceMock<MockWiFiStateMachine> state_machine;

    std::unique_ptr<WiFiMessageProcessor> processor;

    void SetUp() override
    {
        processor = std::make_unique<WiFiMessageProcessor>(
            driver_hal, storage, state_machine, sync_manager);
    }
};

// =============================================================================
// process_message — command dispatch
// =============================================================================

TEST_F(MessageProcessorTest, ProcessMessageStartCallsHandleStart)
{
    EXPECT_CALL(driver_hal, wifi_start()).WillOnce(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    processor->process_message(msg, State::INITIALIZED);
}

TEST_F(MessageProcessorTest, ProcessMessageStopCallsHandleStop)
{
    EXPECT_CALL(driver_hal, wifi_stop()).WillOnce(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::STOP;
    processor->process_message(msg, State::STARTED);
}

TEST_F(MessageProcessorTest, ProcessMessageConnectCallsHandleConnect)
{
    EXPECT_CALL(driver_hal, wifi_connect()).WillOnce(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::CONNECT;
    processor->process_message(msg, State::STARTED);
}

TEST_F(MessageProcessorTest, ProcessMessageDisconnectCallsHandleDisconnect)
{
    EXPECT_CALL(driver_hal, wifi_disconnect()).WillOnce(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::DISCONNECT;
    processor->process_message(msg, State::CONNECTED_NO_IP);
}

TEST_F(MessageProcessorTest, ProcessMessageExitDoesNotCallDriver)
{
    EXPECT_CALL(driver_hal, wifi_start()).Times(0);
    EXPECT_CALL(driver_hal, wifi_stop()).Times(0);
    EXPECT_CALL(driver_hal, wifi_connect()).Times(0);
    EXPECT_CALL(driver_hal, wifi_disconnect()).Times(0);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::EXIT;
    processor->process_message(msg, State::INITIALIZED);
}

TEST_F(MessageProcessorTest, ProcessMessageCommandResetsRetries)
{
    EXPECT_CALL(state_machine, reset_retries()).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    processor->process_message(msg, State::INITIALIZED);
}

TEST_F(MessageProcessorTest, ProcessMessageExitDoesNotResetRetries)
{
    EXPECT_CALL(state_machine, reset_retries()).Times(0);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::EXIT;
    processor->process_message(msg, State::INITIALIZED);
}

// =============================================================================
// handle_start
// =============================================================================

TEST_F(MessageProcessorTest, HandleStartTransitionsToStarting)
{
    EXPECT_CALL(state_machine, transition_to(State::STARTING));
    ON_CALL(driver_hal, wifi_start()).WillByDefault(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    processor->process_message(msg, State::INITIALIZED);
}

TEST_F(MessageProcessorTest, HandleStartDriverFailureSetsStartFailedBit)
{
    EXPECT_CALL(driver_hal, wifi_start()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(sync_manager, set_bits(START_FAILED_BIT)).Times(1);

    // Should transition to STARTING then back to INITIALIZED on failure
    EXPECT_CALL(state_machine, transition_to(State::STARTING)).Times(1);
    EXPECT_CALL(state_machine, transition_to(State::INITIALIZED)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    processor->process_message(msg, State::INITIALIZED);
}

// =============================================================================
// handle_stop
// =============================================================================

TEST_F(MessageProcessorTest, HandleStopDriverFailureSetsStopFailedBit)
{
    EXPECT_CALL(driver_hal, wifi_stop()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(sync_manager, set_bits(STOP_FAILED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::STOP;
    processor->process_message(msg, State::STARTED);
}

// =============================================================================
// handle_connect
// =============================================================================

TEST_F(MessageProcessorTest, HandleConnectDriverFailureSetsConnectFailedBit)
{
    EXPECT_CALL(driver_hal, wifi_connect()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::CONNECT;
    processor->process_message(msg, State::STARTED);
}

// =============================================================================
// handle_disconnect
// =============================================================================

TEST_F(MessageProcessorTest, HandleDisconnectFromWaitingReconnectTransitionsToDisconnected)
{
    EXPECT_CALL(state_machine, transition_to(State::DISCONNECTED)).Times(1);
    EXPECT_CALL(sync_manager, set_bits(DISCONNECTED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::DISCONNECT;
    processor->process_message(msg, State::WAITING_RECONNECT);
}

TEST_F(MessageProcessorTest, HandleDisconnectNormalPathTransitionsToDisconnecting)
{
    EXPECT_CALL(state_machine, transition_to(State::DISCONNECTING)).Times(1);
    ON_CALL(driver_hal, wifi_disconnect()).WillByDefault(Return(ESP_OK));

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::DISCONNECT;
    processor->process_message(msg, State::CONNECTED_NO_IP);
}

// =============================================================================
// on_idle_tick
// =============================================================================

TEST_F(MessageProcessorTest, OnIdleTickInWaitingReconnectWithValidCredentialsStartsConnect)
{
    ON_CALL(storage, is_valid()).WillByDefault(Return(true));
    EXPECT_CALL(state_machine, transition_to(State::CONNECTING)).Times(1);
    EXPECT_CALL(driver_hal, wifi_connect()).Times(1);

    processor->on_idle_tick(State::WAITING_RECONNECT);
}

TEST_F(MessageProcessorTest, OnIdleTickInWaitingReconnectWithInvalidCredentialsTransitionsToDisconnected)
{
    ON_CALL(storage, is_valid()).WillByDefault(Return(false));
    EXPECT_CALL(state_machine, transition_to(State::DISCONNECTED)).Times(1);
    EXPECT_CALL(driver_hal, wifi_connect()).Times(0);

    processor->on_idle_tick(State::WAITING_RECONNECT);
}

TEST_F(MessageProcessorTest, OnIdleTickInOtherStatesDoesNothing)
{
    EXPECT_CALL(state_machine, transition_to(_)).Times(0);
    EXPECT_CALL(driver_hal, wifi_connect()).Times(0);

    processor->on_idle_tick(State::STARTED);
}
