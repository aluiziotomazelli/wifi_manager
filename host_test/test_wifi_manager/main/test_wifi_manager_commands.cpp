// host_test/test_wifi_manager/main/test_wifi_manager_commands.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_manager.hpp"

#include "mock_wifi_bootstrapper.hpp"
#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_message_processor.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"

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
    ON_CALL(*storage, is_valid()).WillByDefault(Return(true));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->connect());
}

TEST_F(WiFiManagerCommandsTest, ConnectAsyncValidateCommandSkipReturnsOk)
{
    skip_command(CommandId::CONNECT);
    ON_CALL(*storage, is_valid()).WillByDefault(Return(true));

    EXPECT_EQ(ESP_OK, manager->connect());
}

TEST_F(WiFiManagerCommandsTest, ConnectAsyncPostsConnectCommand)
{
    allow_command(CommandId::CONNECT);
    ON_CALL(*storage, is_valid()).WillByDefault(Return(true));

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::CONNECT)))
        .WillOnce(Return(ESP_OK));

    manager->connect();
}
TEST_F(WiFiManagerCommandsTest, ConnectAsyncWithInvalidCredentialsReturnsError)
{
    allow_command(CommandId::CONNECT);
    ON_CALL(*storage, is_valid()).WillByDefault(Return(false));

    EXPECT_EQ(ESP_ERR_WIFI_PASSWORD, manager->connect());
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

// =============================================================================
// post_message()
// =============================================================================

TEST_F(WiFiManagerCommandsTest, PostMessageSyncUninitalizedPropagatesError)
{
    allow_command(CommandId::STOP);

    EXPECT_CALL(*sync_manager, is_initialized()).Times(2).WillOnce(Return(true)).WillOnce(Return(false));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->start());
}

TEST_F(WiFiManagerCommandsTest, PostMessageFailsPropagatesError)
{
    allow_command(CommandId::START);

    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::START)))
        .WillOnce(Return(ESP_FAIL));

    EXPECT_EQ(ESP_FAIL, manager->start());
}
