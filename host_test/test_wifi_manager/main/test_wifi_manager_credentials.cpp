// host_test/test_wifi_manager/main/test_wifi_manager_credentials.cpp
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

class WiFiManagerCredentialsTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> *driver_hal;
    NiceMock<MockWiFiConfigStorage> *storage;
    NiceMock<MockWiFiStateMachine> *state_machine;
    NiceMock<MockWiFiMessageProcessor> *processor;

    std::unique_ptr<WiFiManager> manager;

    State current_state = State::INITIALIZED;

    void SetUp() override
    {
        auto driver_hal_owned = std::make_unique<NiceMock<MockWiFiDriverHAL>>();
        auto storage_owned = std::make_unique<NiceMock<MockWiFiConfigStorage>>();
        auto state_machine_owned = std::make_unique<NiceMock<MockWiFiStateMachine>>();
        auto processor_owned = std::make_unique<NiceMock<MockWiFiMessageProcessor>>();

        driver_hal = driver_hal_owned.get();
        storage = storage_owned.get();
        state_machine = state_machine_owned.get();
        processor = processor_owned.get();

        ON_CALL(*state_machine, get_current_state()).WillByDefault(ReturnPointee(&current_state));
        ON_CALL(*state_machine, transition_to(_)).WillByDefault(SaveArg<0>(&current_state));

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::make_unique<NiceMock<MockWiFiSyncManager>>(),
            std::move(state_machine_owned),
            std::make_unique<NiceMock<MockWiFiBootstrapper>>(),
            std::move(processor_owned));
    }
};

// ==========================================================================
// SetCredentials
// ==========================================================================

TEST_F(WiFiManagerCredentialsTest, SetCredentialsUnitializedStateReturnsError)
{
    current_state = State::UNINITIALIZED;                                       // unitialized state
    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->set_credentials("ssid", "pass")); // must return error
}

TEST_F(WiFiManagerCredentialsTest, SetCredentialsStateMachineActiveCallsDriverDisconnect)
{
    ON_CALL(*state_machine, is_active()).WillByDefault(Return(true)); // state machine is active
    EXPECT_CALL(*driver_hal, wifi_disconnect()).Times(1);             // must call driver disconnect
    EXPECT_CALL(*storage, save_credentials(_, _)).Times(1);           // must call storage

    EXPECT_EQ(ESP_OK, manager->set_credentials("ssid", "pass"));
}

TEST_F(WiFiManagerCredentialsTest, SetCredentialsStateMachineInactiveDoesNotCallDriverDisconnect)
{
    ON_CALL(*state_machine, is_active()).WillByDefault(Return(false)); // state machine is inactive
    EXPECT_CALL(*driver_hal, wifi_disconnect()).Times(0);              // must not call driver disconnect
    EXPECT_CALL(*storage, save_credentials(_, _)).Times(1);            // must call storage

    EXPECT_EQ(ESP_OK, manager->set_credentials("ssid", "pass"));
}

TEST_F(WiFiManagerCredentialsTest, SetCredentialsSetCredentialsPropagatesError)
{
    EXPECT_CALL(*storage, save_credentials(_, _)).WillOnce(Return(ESP_FAIL));
    EXPECT_EQ(ESP_FAIL, manager->set_credentials("ssid", "pass"));
}

// ==========================================================================
// ClearCredentials
// ==========================================================================

TEST_F(WiFiManagerCredentialsTest, ClearCredentialsUnitializedStateReturnsError)
{
    current_state = State::UNINITIALIZED;                           // unitialized state
    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->clear_credentials()); // must return error
}
TEST_F(WiFiManagerCredentialsTest, ClearCredentialsCallsResetRetries)
{
    EXPECT_CALL(*storage, clear_credentials()).Times(1).WillOnce(Return(ESP_OK)); // storage called
    EXPECT_CALL(*state_machine, reset_retries()).Times(1);                        // must call reset retries
    EXPECT_EQ(ESP_OK, manager->clear_credentials());
}

TEST_F(WiFiManagerCredentialsTest, ClearCredentialsPropagatesError)
{
    EXPECT_CALL(*storage, clear_credentials()).WillOnce(Return(ESP_FAIL)); // storage error
    EXPECT_CALL(*state_machine, reset_retries()).Times(0);                 // must not call reset retries
    EXPECT_EQ(ESP_FAIL, manager->clear_credentials());                     // must return error
}

// ==========================================================================
// FactoryReset
// ==========================================================================

TEST_F(WiFiManagerCredentialsTest, FactoryResetUnitializedStateReturnsError)
{
    current_state = State::UNINITIALIZED;                       // unitialized state
    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->factory_reset()); // must return error
}

TEST_F(WiFiManagerCredentialsTest, FactoryResetPropagatesError)
{
    EXPECT_CALL(*storage, factory_reset()).Times(1).WillOnce(Return(ESP_FAIL)); // factory reset error
    EXPECT_CALL(*state_machine, reset_retries()).Times(1);                      // must call reset retries
    EXPECT_CALL(*state_machine, transition_to(State::INITIALIZED)).Times(1);    // must call transition
    EXPECT_EQ(ESP_FAIL, manager->factory_reset());
}

TEST_F(WiFiManagerCredentialsTest, FactoryResetPropagatesSuccess)
{
    EXPECT_CALL(*storage, factory_reset()).Times(1).WillOnce(Return(ESP_OK)); // factory reset error
    EXPECT_CALL(*state_machine, reset_retries()).Times(1);                    // must call reset retries
    EXPECT_CALL(*state_machine, transition_to(State::INITIALIZED)).Times(1);  // must call transition
    EXPECT_EQ(ESP_OK, manager->factory_reset());
}

// ==========================================================================
// Get Credentials
// ==========================================================================

TEST_F(WiFiManagerCredentialsTest, GetCredentialsReturnsCredentials)
{
    EXPECT_CALL(*storage, load_credentials(_, _))
        .WillOnce(DoAll(
            SetArgReferee<0>("ssid"),
            SetArgReferee<1>("pass"),
            Return(ESP_OK))); // load credentials returns ssid, pass, and success

    std::string ssid;
    std::string password;

    EXPECT_EQ(ESP_OK, manager->get_credentials(ssid, password)); // must return success
    EXPECT_EQ("ssid", ssid);                                     // must match ssid
    EXPECT_EQ("pass", password);                                 // must match password
}

TEST_F(WiFiManagerCredentialsTest, GetCredentialsPropagatesError)
{
    EXPECT_CALL(*storage, load_credentials(_, _)).WillOnce(Return(ESP_FAIL)); // load credentials error
    std::string ssid;
    std::string password;
    EXPECT_EQ(ESP_FAIL, manager->get_credentials(ssid, password)); // must return error
}

// ==========================================================================
// Credential is Valid
// ==========================================================================

TEST_F(WiFiManagerCredentialsTest, IsCredentialsValidReturnsTrue)
{
    EXPECT_CALL(*storage, is_valid()).WillOnce(Return(true)); // storage returns true
    EXPECT_TRUE(manager->is_credentials_valid());             // must return true
}

TEST_F(WiFiManagerCredentialsTest, IsCredentialsValidReturnsFalse)
{
    EXPECT_CALL(*storage, is_valid()).WillOnce(Return(false)); // storage returns false
    EXPECT_FALSE(manager->is_credentials_valid());             // must return false
}
