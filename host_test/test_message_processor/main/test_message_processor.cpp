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
        processor = std::make_unique<WiFiMessageProcessor>(driver_hal, storage, state_machine, sync_manager);
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

TEST_F(MessageProcessorTest, HandleDisconnectDriverFailureSetsDisconnectFailedBit)
{
    EXPECT_CALL(driver_hal, wifi_disconnect()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);

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

// =============================================================================
// handle_event
// =============================================================================

TEST_F(MessageProcessorTest, HandleEventDifferentStateCallsResolveEventOnce)
{
    // Different state than the one in the event
    IWiFiStateMachine::EventOutcome neutral = {State::DISCONNECTED, 0};
    // Should call resolve_event once
    EXPECT_CALL(state_machine, resolve_event(EventId::STA_DISCONNECTED)).Times(1).WillOnce(Return(neutral));

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    processor->process_message(msg, State::DISCONNECTING);
}

TEST_F(MessageProcessorTest, HandleEventSetBitOnOutcomeEventCallsSetBits)
{
    // Using CONNECTED_BIT to test the bit setting functionality
    IWiFiStateMachine::EventOutcome outcome = {State::CONNECTED_GOT_IP, CONNECTED_BIT};
    EXPECT_CALL(state_machine, resolve_event(EventId::GOT_IP)).WillOnce(Return(outcome));
    EXPECT_CALL(sync_manager, set_bits(CONNECTED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::GOT_IP;
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventStaDisconnectedInDisconnectingState)
{
    // Sets an neutral EventOutcome transition to DISCONNECTING and 0 bits
    // IWiFiStateMachine::EventOutcome neutral = {State::DISCONNECTING, 0};
    // handle_event will call resolv_event and returno neutral outcome
    // EXPECT_CALL(state_machine, resolve_event(EventId::STA_DISCONNECTED)).WillOnce(Return(neutral));
    // handle_event will set bits DISCONNECTED_BIT | CONNECT_FAILED_BIT
    EXPECT_CALL(sync_manager, set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT)).Times(1);

    // process_message will call handle_event
    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    processor->process_message(msg, State::DISCONNECTING);
}

TEST_F(MessageProcessorTest, HandleEventStaDisconnectedInStoppingState)
{
    IWiFiStateMachine::EventOutcome neutral = {State::STOPPING, 0};
    EXPECT_CALL(state_machine, resolve_event(EventId::STA_DISCONNECTED)).WillOnce(Return(neutral));
    EXPECT_CALL(sync_manager, set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    processor->process_message(msg, State::STOPPING);
}

TEST_F(MessageProcessorTest, HandleEventStaDisconnectedWithStateMachineInactive)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(false));

    IWiFiStateMachine::EventOutcome neutral = {State::DISCONNECTED, 0};
    EXPECT_CALL(state_machine, resolve_event(EventId::STA_DISCONNECTED)).WillOnce(Return(neutral));
    EXPECT_CALL(sync_manager, set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    processor->process_message(msg, State::DISCONNECTED);
}

TEST_F(MessageProcessorTest, HandleEventStaDisconnectedWithReasonAssocLeave)
{
    IWiFiStateMachine::EventOutcome neutral = {State::CONNECTED_GOT_IP, 0};
    EXPECT_CALL(state_machine, resolve_event(EventId::STA_DISCONNECTED)).Times(1).WillOnce(Return(neutral));
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true));

    EXPECT_CALL(sync_manager, set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT)).Times(1);
    EXPECT_CALL(state_machine, transition_to(State::DISCONNECTED)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    msg.reason = WIFI_REASON_ASSOC_LEAVE;
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventDisconnectAuthFailNotSuspectFailure)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true));               // state machine is active
    EXPECT_CALL(state_machine, handle_suspect_failure(_)).WillOnce(Return(false)); // no suspect failure
    EXPECT_CALL(storage, save_valid_flag(_)).Times(0);                             // should not save valid flag
    EXPECT_CALL(state_machine, calculate_next_backoff(_)).Times(1);                // should calculate next backoff
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);              // should set bits

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    msg.reason = WIFI_REASON_AUTH_FAIL;
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventDisconnectAuthFailSuspectFailure)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true));              // state machine is active
    EXPECT_CALL(state_machine, handle_suspect_failure(_)).WillOnce(Return(true)); // suspect failure
    EXPECT_CALL(storage, save_valid_flag(_)).Times(1);                            // should save valid flag
    EXPECT_CALL(state_machine, calculate_next_backoff(_)).Times(0);               // should not calculate next backoff
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);             // should set bits

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    msg.reason = WIFI_REASON_AUTH_FAIL;
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventDisconnectOtherReasonValidCredentials)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true));  // state machine is active
    ON_CALL(storage, is_valid()).WillByDefault(Return(true));         // valid credentials
    EXPECT_CALL(state_machine, calculate_next_backoff(_)).Times(1);   // should calculate next backoff
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1); // should set bits

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    msg.reason = WIFI_REASON_NO_AP_FOUND; // Generic reason not in our failure special cases
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventDisconnectOtherReasonInvalidCredentials)
{
    IWiFiStateMachine::EventOutcome neutral = {State::CONNECTED_GOT_IP, 0};
    ON_CALL(state_machine, resolve_event(_)).WillByDefault(Return(neutral));

    ON_CALL(state_machine, is_active()).WillByDefault(Return(true));         // state machine is active
    ON_CALL(storage, is_valid()).WillByDefault(Return(false));               // invalid credentials
    EXPECT_CALL(state_machine, calculate_next_backoff(_)).Times(0);          // should not calculate next backoff
    EXPECT_CALL(state_machine, transition_to(State::DISCONNECTED)).Times(1); // should transition to disconnected
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);        // should set bits

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::STA_DISCONNECTED;
    msg.reason = WIFI_REASON_NO_AP_FOUND; // Generic reason not in our failure special cases
    processor->process_message(msg, State::CONNECTED_GOT_IP);
}

TEST_F(MessageProcessorTest, HandleEventGotIpWithValidCredentials)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true)); // state machine is active
    EXPECT_CALL(state_machine, reset_retries()).Times(1);            // should reset retries
    ON_CALL(storage, is_valid()).WillByDefault(Return(true));        // valid credentials
    EXPECT_CALL(storage, save_valid_flag(true)).Times(0);            // should not save valid flag

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::GOT_IP;
    processor->process_message(msg, State::CONNECTED_NO_IP);
}

TEST_F(MessageProcessorTest, HandleEventGotIpWithInvalidCredentials)
{
    ON_CALL(state_machine, is_active()).WillByDefault(Return(true)); // state machine is active
    EXPECT_CALL(state_machine, reset_retries()).Times(1);            // should reset retries
    ON_CALL(storage, is_valid()).WillByDefault(Return(false));       // invalid credentials
    EXPECT_CALL(storage, save_valid_flag(true)).Times(1);            // should save valid flag

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::GOT_IP;
    processor->process_message(msg, State::CONNECTED_NO_IP);
}

TEST_F(MessageProcessorTest, HandleEventDefaultSwitch)
{
    EXPECT_CALL(sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(0); // should not set bits
    EXPECT_CALL(state_machine, reset_retries()).Times(0);             // should not reset retries

    wifi_manager::Message msg = {};
    msg.type = MessageType::EVENT;
    msg.event = EventId::COUNT; // invalid event
    processor->process_message(msg, State::CONNECTED_NO_IP);
}