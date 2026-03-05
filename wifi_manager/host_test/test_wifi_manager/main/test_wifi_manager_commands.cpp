// host_test/test_wifi_manager/main/test_wifi_manager_commands.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_manager.hpp"

#include "mock_wifi_bootstrapper.hpp"
#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"
#include "mock_wifi_message_processor.hpp"

using namespace wifi_manager;
using namespace testing;

// =============================================================================
// Fixture
// =============================================================================

class WiFiManagerCommandsTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> *driver_hal;
    NiceMock<MockWiFiConfigStorage> *storage;
    NiceMock<MockWiFiSyncManager> *sync_manager;
    NiceMock<MockWiFiStateMachine> *state_machine;
    NiceMock<MockWiFiBootstrapper> *bootstrapper;
    NiceMock<MockWiFiMessageProcessor> *processor;

    std::unique_ptr<WiFiManager> manager;

    State current_state = State::INITIALIZED;

    void SetUp() override
    {
        auto driver_hal_owned = std::make_unique<NiceMock<MockWiFiDriverHAL>>();
        auto storage_owned = std::make_unique<NiceMock<MockWiFiConfigStorage>>();
        auto sync_manager_owned = std::make_unique<NiceMock<MockWiFiSyncManager>>();
        auto state_machine_owned = std::make_unique<NiceMock<MockWiFiStateMachine>>();
        auto bootstrapper_owned = std::make_unique<NiceMock<MockWiFiBootstrapper>>();
        auto processor_owned = std::make_unique<NiceMock<MockWiFiMessageProcessor>>();

        driver_hal = driver_hal_owned.get();
        storage = storage_owned.get();
        sync_manager = sync_manager_owned.get();
        state_machine = state_machine_owned.get();
        bootstrapper = bootstrapper_owned.get();
        processor = processor_owned.get();

        ON_CALL(*state_machine, get_current_state()).WillByDefault(ReturnPointee(&current_state));
        ON_CALL(*state_machine, transition_to(_)).WillByDefault(SaveArg<0>(&current_state));

        // Default: sync_manager is initialized and post_message succeeds
        ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(true));
        ON_CALL(*sync_manager, post_message(_)).WillByDefault(Return(ESP_OK));

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::move(sync_manager_owned),
            std::move(state_machine_owned),
            std::move(bootstrapper_owned),
            std::move(processor_owned));
    }

    void TearDown() override { current_state = State::INITIALIZED; }

    // Helper: configure validate_command to allow a specific command
    void allow_command(CommandId cmd)
    {
        ON_CALL(*state_machine, validate_command(cmd)).WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));
    }

    // Helper: configure validate_command to skip a specific command
    void skip_command(CommandId cmd)
    {
        ON_CALL(*state_machine, validate_command(cmd)).WillByDefault(Return(IWiFiStateMachine::Action::SKIP));
    }

    // Helper: configure validate_command to reject a specific command
    void reject_command(CommandId cmd)
    {
        ON_CALL(*state_machine, validate_command(cmd)).WillByDefault(Return(IWiFiStateMachine::Action::ERROR));
    }
};

// // =============================================================================
// // process_message — command dispatch
// // =============================================================================

// TEST_F(WiFiManagerCommandsTest, ProcessMessageStartCallsHandleStart)
// {
//     // START command must call wifi_start on the driver
//     EXPECT_CALL(*driver_hal, wifi_start()).WillOnce(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::START;
//     manager->process_message(msg, State::INITIALIZED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageStopCallsHandleStop)
// {
//     EXPECT_CALL(*driver_hal, wifi_stop()).WillOnce(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::STOP;
//     manager->process_message(msg, State::STARTED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageConnectCallsHandleConnect)
// {
//     EXPECT_CALL(*driver_hal, wifi_connect()).WillOnce(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::CONNECT;
//     manager->process_message(msg, State::STARTED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageDisconnectCallsHandleDisconnect)
// {
//     EXPECT_CALL(*driver_hal, wifi_disconnect()).WillOnce(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::DISCONNECT;
//     manager->process_message(msg, State::CONNECTED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageExitDoesNotCallDriver)
// {
//     // EXIT is handled by wifi_task directly — process_message should ignore it
//     EXPECT_CALL(*driver_hal, wifi_start()).Times(0);
//     EXPECT_CALL(*driver_hal, wifi_stop()).Times(0);
//     EXPECT_CALL(*driver_hal, wifi_connect()).Times(0);
//     EXPECT_CALL(*driver_hal, wifi_disconnect()).Times(0);

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::EXIT;
//     manager->process_message(msg, State::INITIALIZED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageCommandResetsRetries)
// {
//     // Any command except EXIT must reset retries
//     EXPECT_CALL(*state_machine, reset_retries()).Times(1);

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::START;
//     manager->process_message(msg, State::INITIALIZED);
// }

// TEST_F(WiFiManagerCommandsTest, ProcessMessageExitDoesNotResetRetries)
// {
//     EXPECT_CALL(*state_machine, reset_retries()).Times(0);

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::EXIT;
//     manager->process_message(msg, State::INITIALIZED);
// }

// =============================================================================
// handle_start
// =============================================================================

// TEST_F(WiFiManagerCommandsTest, HandleStartTransitionsToStarting)
// {
//     ON_CALL(*driver_hal, wifi_start()).WillByDefault(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     manager->handle_start(msg, State::INITIALIZED);

//     // On success, state should have passed through STARTING
//     // (transition_to is called with STARTING before wifi_start)
//     // Since SaveArg captures the last call, verify via InOrder
//     InOrder order;
//     // Rebuild with strict mock to verify order
// }

// TEST_F(WiFiManagerCommandsTest, HandleStartDriverFailureSetsStartFailedBit)
// {
//     // wifi_start fails — must set START_FAILED_BIT and restore state
//     EXPECT_CALL(*driver_hal, wifi_start()).WillOnce(Return(ESP_FAIL));
//     EXPECT_CALL(*sync_manager, set_bits(START_FAILED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_start(msg, State::INITIALIZED);

//     // State must be restored to the previous state
//     EXPECT_EQ(State::INITIALIZED, current_state);
// }

// =============================================================================
// handle_stop
// =============================================================================

// TEST_F(WiFiManagerCommandsTest, HandleStopDriverFailureSetsStopFailedBit)
// {
//     EXPECT_CALL(*driver_hal, wifi_stop()).WillOnce(Return(ESP_FAIL));
//     EXPECT_CALL(*sync_manager, set_bits(STOP_FAILED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_stop(msg, State::STARTED);

//     EXPECT_EQ(State::STARTED, current_state);
// }

// =============================================================================
// handle_connect
// =============================================================================

// TEST_F(WiFiManagerCommandsTest, HandleConnectDriverFailureSetsConnectFailedBit)
// {
//     EXPECT_CALL(*driver_hal, wifi_connect()).WillOnce(Return(ESP_FAIL));
//     EXPECT_CALL(*sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_connect(msg, State::STARTED);

//     EXPECT_EQ(State::STARTED, current_state);
// }

// =============================================================================
// handle_disconnect
// =============================================================================

// TEST_F(WiFiManagerCommandsTest, HandleDisconnectFromWaitingReconnectTransitionsToDisconnected)
// {
//     // From WAITING_RECONNECT, disconnect must transition to DISCONNECTED immediately
//     EXPECT_CALL(*sync_manager, set_bits(DISCONNECTED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_disconnect(msg, State::WAITING_RECONNECT);

//     EXPECT_EQ(State::DISCONNECTED, current_state);
// }

// TEST_F(WiFiManagerCommandsTest, HandleDisconnectFromConnectingTransitionsToDisconnected)
// {
//     // From CONNECTING, disconnect must also transition to DISCONNECTED immediately
//     EXPECT_CALL(*sync_manager, set_bits(DISCONNECTED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_disconnect(msg, State::CONNECTING);

//     EXPECT_EQ(State::DISCONNECTED, current_state);
// }

// TEST_F(WiFiManagerCommandsTest, HandleDisconnectNormalPathTransitionsToDisconnecting)
// {
//     ON_CALL(*driver_hal, wifi_disconnect()).WillByDefault(Return(ESP_OK));

//     wifi_manager::Message msg = {};
//     manager->handle_disconnect(msg, State::CONNECTED_NO_IP);

//     // Normal path: must transition through DISCONNECTING
//     EXPECT_EQ(State::DISCONNECTING, current_state);
// }

// TEST_F(WiFiManagerCommandsTest, HandleDisconnectDriverFailureSetsConnectFailedBit)
// {
//     EXPECT_CALL(*driver_hal, wifi_disconnect()).WillOnce(Return(ESP_FAIL));
//     EXPECT_CALL(*sync_manager, set_bits(CONNECT_FAILED_BIT)).Times(1);

//     wifi_manager::Message msg = {};
//     manager->handle_disconnect(msg, State::CONNECTED_NO_IP);

//     EXPECT_EQ(State::CONNECTED_NO_IP, current_state);
// }

// =============================================================================
// start() async — validate_command guard
// =============================================================================

TEST_F(WiFiManagerCommandsTest, StartAsyncSyncManagerNotInitializedReturnsInvalidState)
{
    ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(false));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->start());
}

TEST_F(WiFiManagerCommandsTest, StartAsyncValidateCommandErrorReturnsInvalidState)
{
    reject_command(CommandId::START);

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->start());
}

TEST_F(WiFiManagerCommandsTest, StartAsyncValidateCommandSkipReturnsOk)
{
    skip_command(CommandId::START);

    EXPECT_EQ(ESP_OK, manager->start());
}

TEST_F(WiFiManagerCommandsTest, StartAsyncPostsStartCommand)
{
    allow_command(CommandId::START);

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::START)))
        .WillOnce(Return(ESP_OK));

    manager->start();
}

// =============================================================================
// stop() async — validate_command guard
// =============================================================================

TEST_F(WiFiManagerCommandsTest, StopAsyncSyncManagerNotInitializedReturnsInvalidState)
{
    ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(false));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->stop());
}

TEST_F(WiFiManagerCommandsTest, StopAsyncValidateCommandErrorReturnsInvalidState)
{
    reject_command(CommandId::STOP);

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->stop());
}

TEST_F(WiFiManagerCommandsTest, StopAsyncValidateCommandSkipReturnsOk)
{
    skip_command(CommandId::STOP);

    EXPECT_EQ(ESP_OK, manager->stop());
}

TEST_F(WiFiManagerCommandsTest, StopAsyncPostsStopCommand)
{
    allow_command(CommandId::STOP);

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::STOP)))
        .WillOnce(Return(ESP_OK));

    manager->stop();
}

// =============================================================================
// connect() async — validate_command guard
// =============================================================================

TEST_F(WiFiManagerCommandsTest, ConnectAsyncSyncManagerNotInitializedReturnsInvalidState)
{
    ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(false));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->connect());
}

TEST_F(WiFiManagerCommandsTest, ConnectAsyncValidateCommandErrorReturnsInvalidState)
{
    reject_command(CommandId::CONNECT);

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->connect());
}

TEST_F(WiFiManagerCommandsTest, ConnectAsyncValidateCommandSkipReturnsOk)
{
    skip_command(CommandId::CONNECT);

    EXPECT_EQ(ESP_OK, manager->connect());
}

TEST_F(WiFiManagerCommandsTest, ConnectAsyncPostsConnectCommand)
{
    allow_command(CommandId::CONNECT);

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::CONNECT)))
        .WillOnce(Return(ESP_OK));

    manager->connect();
}

// =============================================================================
// disconnect() async — validate_command guard
// =============================================================================

TEST_F(WiFiManagerCommandsTest, DisconnectAsyncSyncManagerNotInitializedReturnsInvalidState)
{
    ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(false));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->disconnect());
}

TEST_F(WiFiManagerCommandsTest, DisconnectAsyncValidateCommandErrorReturnsInvalidState)
{
    reject_command(CommandId::DISCONNECT);

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->disconnect());
}

TEST_F(WiFiManagerCommandsTest, DisconnectAsyncValidateCommandSkipReturnsOk)
{
    skip_command(CommandId::DISCONNECT);

    EXPECT_EQ(ESP_OK, manager->disconnect());
}

TEST_F(WiFiManagerCommandsTest, DisconnectAsyncPostsDisconnectCommand)
{
    allow_command(CommandId::DISCONNECT);

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::DISCONNECT)))
        .WillOnce(Return(ESP_OK));

    manager->disconnect();
}

// // =============================================================================
// // post_message
// // =============================================================================

// TEST_F(WiFiManagerCommandsTest, PostMessageSyncManagerNotInitializedReturnsInvalidState)
// {
//     ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(false));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::START;

//     EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->post_message(msg, false));
// }

// TEST_F(WiFiManagerCommandsTest, PostMessageQueueFullLogsAndReturnsError)
// {
//     ON_CALL(*sync_manager, post_message(_)).WillByDefault(Return(ESP_ERR_NO_MEM));

//     wifi_manager::Message msg = {};
//     msg.type = MessageType::COMMAND;
//     msg.cmd = CommandId::START;

//     EXPECT_EQ(ESP_ERR_NO_MEM, manager->post_message(msg, false));
// }