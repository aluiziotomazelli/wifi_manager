// host_test/test_wifi_manager/main/test_wifi_manager_init.cpp

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

class WiFiManagerInitTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> *driver_hal;
    NiceMock<MockWiFiConfigStorage> *storage;
    NiceMock<MockWiFiSyncManager> *sync_manager;
    NiceMock<MockWiFiStateMachine> *state_machine;
    NiceMock<MockWiFiBootstrapper> *bootstrapper;
    NiceMock<MockWiFiMessageProcessor> *processor;

    std::unique_ptr<WiFiManager> manager;

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

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::move(sync_manager_owned),
            std::move(state_machine_owned),
            std::move(bootstrapper_owned),
            std::move(processor_owned));
    }

    void TearDown() override { current_state = State::UNINITIALIZED; }

    State current_state = State::UNINITIALIZED;

    void setup_successful_init() { ON_CALL(*bootstrapper, init(_, _, _, _, _)).WillByDefault(Return(ESP_OK)); }
};

TEST_F(WiFiManagerInitTest, InitAlreadyInitializedReturnsOkWithoutCallingBootstrapper)
{
    setup_successful_init();

    // First init succeeds
    ASSERT_EQ(ESP_OK, manager->init());
    ASSERT_EQ(State::INITIALIZED, manager->get_state());

    // Second init must return ESP_OK immediately without calling bootstrapper again
    EXPECT_CALL(*bootstrapper, init(_, _, _, _, _)).Times(0);

    EXPECT_EQ(ESP_OK, manager->init());
}

TEST_F(WiFiManagerInitTest, InitBootstrapperFailurePropagatesError)
{
    EXPECT_CALL(*bootstrapper, init(_, _, _, _, _)).WillOnce(Return(ESP_FAIL));

    EXPECT_EQ(ESP_FAIL, manager->init());

    // State must be rolled back to UNINITIALIZED
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitSuccessTransitionsToInitialized)
{
    setup_successful_init();

    EXPECT_EQ(ESP_OK, manager->init());
    EXPECT_EQ(State::INITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, DeinitWhenUninitializedReturnsOkImmediately)
{
    // State is UNINITIALIZED — bootstrapper deinit should NOT be called
    EXPECT_CALL(*bootstrapper, deinit(_)).Times(0);

    EXPECT_EQ(ESP_OK, manager->deinit());
}

TEST_F(WiFiManagerInitTest, DeinitAfterInitCallsBootstrapperDeinit)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());

    EXPECT_CALL(*bootstrapper, deinit(_)).WillOnce(Return(ESP_OK));

    manager->deinit();
}

TEST_F(WiFiManagerInitTest, DeinitTransitionsToUninitialized)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());
    ASSERT_EQ(State::INITIALIZED, manager->get_state());

    ON_CALL(*bootstrapper, deinit(_)).WillByDefault(Return(ESP_OK));
    manager->deinit();

    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, DeinitWithActiveWifiPostsStopCommand)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());

    // Simulate active WiFi
    ON_CALL(*state_machine, is_active()).WillByDefault(Return(true));

    // validate_command for STOP must return EXECUTE for stop() to proceed
    ON_CALL(*state_machine, validate_command(CommandId::STOP))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    ON_CALL(*sync_manager, is_initialized()).WillByDefault(Return(true));
    ON_CALL(*bootstrapper, deinit(_)).WillByDefault(Return(ESP_OK));

    // stop(2000) internally calls post_message with STOP command
    EXPECT_CALL(*sync_manager, post_message(Field(&wifi_manager::Message::cmd, CommandId::STOP))).Times(1);

    manager->deinit();
}

TEST_F(WiFiManagerInitTest, InitWithCustomConfigForwardsParametersToBootstrapper)
{
    Config custom_config = {
        .task_stack_size = 6144,
        .task_priority = 6
    };

    EXPECT_CALL(*bootstrapper, init(_, _, _, 6144, 6))
        .WillOnce(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, manager->init(custom_config));
}