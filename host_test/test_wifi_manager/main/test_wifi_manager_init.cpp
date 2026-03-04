// host_test/test_wifi_manager/main/test_wifi_manager_init.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "esp_timer_hal.hpp"
#include "wifi_manager.hpp"
#include "wifi_state_machine.hpp"
#include "wifi_sync_manager.hpp"

#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"

using namespace wifi_manager;
using namespace testing;

// =============================================================================
// Fixture
// =============================================================================

class WiFiManagerInitTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> *driver_hal;
    NiceMock<MockWiFiConfigStorage> *storage;
    WiFiSyncManager *sync_manager;
    WiFiStateMachine *state_machine;

    std::unique_ptr<WiFiManager> manager;

    void SetUp() override
    {
        auto driver_hal_owned = std::make_unique<NiceMock<MockWiFiDriverHAL>>();
        auto storage_owned = std::make_unique<NiceMock<MockWiFiConfigStorage>>();
        auto sync_manager_owned = std::make_unique<WiFiSyncManager>();
        auto timer_hal_owned = std::make_unique<EspTimerHAL>();
        auto state_machine_owned = std::make_unique<WiFiStateMachine>(*timer_hal_owned);

        driver_hal = driver_hal_owned.get();
        storage = storage_owned.get();
        sync_manager = sync_manager_owned.get();
        state_machine = state_machine_owned.get();

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::move(sync_manager_owned),
            std::move(state_machine_owned));
    }

    void TearDown() override
    {
        // Send EXIT to stop wifi_task cleanly before mocks are destroyed
        if (sync_manager->is_initialized()) {
            wifi_manager::Message msg = {};
            msg.type = wifi_manager::MessageType::COMMAND;
            msg.cmd = wifi_manager::CommandId::EXIT;
            sync_manager->post_message(msg);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Helper: set up all happy-path ON_CALLs for a full successful init()
    void setup_successful_init()
    {
        ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
        ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
        ON_CALL(*driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));

        // Return nullptr to trigger the create path
        ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
        ON_CALL(*driver_hal, netif_create_default_wifi_sta())
            .WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));

        ON_CALL(*driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(*driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(*driver_hal, event_handler_instance_register(_, _, _, _, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(*storage, ensure_config_fallback()).WillByDefault(Return(ESP_OK));
    }
};

// =============================================================================
// init — guard
// =============================================================================

TEST_F(WiFiManagerInitTest, InitAlreadyInitializedReturnsOkWithoutCallingStorage)
{
    setup_successful_init();

    // First init succeeds
    ASSERT_EQ(ESP_OK, manager->init());
    ASSERT_EQ(State::INITIALIZED, manager->get_state());

    // Second init must return ESP_OK immediately without calling storage again
    EXPECT_CALL(*storage, init()).Times(0);
    EXPECT_CALL(*driver_hal, netif_init()).Times(0);

    EXPECT_EQ(ESP_OK, manager->init());
}

// =============================================================================
// init — failure paths
// =============================================================================

TEST_F(WiFiManagerInitTest, InitStorageFailurePropagatesError)
{
    EXPECT_CALL(*storage, init()).WillOnce(Return(ESP_FAIL));

    // Must not proceed to HAL calls
    EXPECT_CALL(*driver_hal, netif_init()).Times(0);

    EXPECT_EQ(ESP_FAIL, manager->init());

    // State must be rolled back to UNINITIALIZED
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitNetifFailureTriggersDeinit)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(*driver_hal, netif_init()).WillOnce(Return(ESP_ERR_NO_MEM));

    EXPECT_EQ(ESP_ERR_NO_MEM, manager->init());
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitEventLoopFailureTriggersDeinit)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(*driver_hal, event_loop_create_default()).WillOnce(Return(ESP_ERR_NO_MEM));

    EXPECT_EQ(ESP_ERR_NO_MEM, manager->init());
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitStaNetifCreateFailsTriggersDeinit)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));

    // Both get and create return nullptr — must fail with ESP_ERR_NO_MEM
    ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
    ON_CALL(*driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(nullptr));

    EXPECT_EQ(ESP_ERR_NO_MEM, manager->init());
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitWifiInitFailureTriggersDeinit)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
    ON_CALL(*driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));

    EXPECT_CALL(*driver_hal, wifi_init(_)).WillOnce(Return(ESP_FAIL));

    EXPECT_EQ(ESP_FAIL, manager->init());
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitWifiSetModeFailureTriggersDeinit)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
    ON_CALL(*driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));
    ON_CALL(*driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));

    EXPECT_CALL(*driver_hal, wifi_set_mode(_)).WillOnce(Return(ESP_FAIL));

    EXPECT_EQ(ESP_FAIL, manager->init());
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

// =============================================================================
// init — ESP_ERR_INVALID_STATE absorption
// =============================================================================

TEST_F(WiFiManagerInitTest, InitNetifAlreadyInitializedContinues)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));

    // ESP_ERR_INVALID_STATE must be absorbed — init must continue
    EXPECT_CALL(*driver_hal, netif_init()).WillOnce(Return(ESP_ERR_INVALID_STATE));

    // event_loop must still be called after absorption
    EXPECT_CALL(*driver_hal, event_loop_create_default()).WillOnce(Return(ESP_OK));

    ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
    ON_CALL(*driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));
    ON_CALL(*driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_handler_instance_register(_, _, _, _, _)).WillByDefault(Return(ESP_OK));
    ON_CALL(*storage, ensure_config_fallback()).WillByDefault(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, manager->init());
}

TEST_F(WiFiManagerInitTest, InitEventLoopAlreadyCreatedContinues)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));

    // ESP_ERR_INVALID_STATE must be absorbed — init must continue
    EXPECT_CALL(*driver_hal, event_loop_create_default()).WillOnce(Return(ESP_ERR_INVALID_STATE));

    // wifi_init must still be called after absorption
    EXPECT_CALL(*driver_hal, wifi_init(_)).WillOnce(Return(ESP_OK));

    ON_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
    ON_CALL(*driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));
    ON_CALL(*driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_handler_instance_register(_, _, _, _, _)).WillByDefault(Return(ESP_OK));
    ON_CALL(*storage, ensure_config_fallback()).WillByDefault(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, manager->init());
}

// =============================================================================
// init — netif reuse
// =============================================================================

TEST_F(WiFiManagerInitTest, InitReusesExistingStaNetif)
{
    ON_CALL(*storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));

    // get returns existing handle — create must NOT be called
    auto existing = reinterpret_cast<esp_netif_t *>(0x1);
    EXPECT_CALL(*driver_hal, netif_get_handle_from_ifkey(_)).WillOnce(Return(existing));
    EXPECT_CALL(*driver_hal, netif_create_default_wifi_sta()).Times(0);

    ON_CALL(*driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
    ON_CALL(*driver_hal, event_handler_instance_register(_, _, _, _, _)).WillByDefault(Return(ESP_OK));
    ON_CALL(*storage, ensure_config_fallback()).WillByDefault(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, manager->init());
}

// =============================================================================
// init — success guarantees
// =============================================================================

TEST_F(WiFiManagerInitTest, InitSuccessTransitionsToInitialized)
{
    setup_successful_init();

    EXPECT_EQ(ESP_OK, manager->init());
    EXPECT_EQ(State::INITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, InitRegistersWifiAndIpEventHandlers)
{
    setup_successful_init();

    // Both WIFI_EVENT and IP_EVENT handlers must be registered
    EXPECT_CALL(*driver_hal, event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, _, _, _))
        .WillOnce(Return(ESP_OK));
    EXPECT_CALL(*driver_hal, event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, _, _, _))
        .WillOnce(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, manager->init());
}

TEST_F(WiFiManagerInitTest, InitCallsEnsureConfigFallback)
{
    setup_successful_init();

    EXPECT_CALL(*storage, ensure_config_fallback()).Times(1);

    EXPECT_EQ(ESP_OK, manager->init());
}

// =============================================================================
// deinit
// =============================================================================

TEST_F(WiFiManagerInitTest, DeinitWhenUninitializedReturnsOkImmediately)
{
    // State is UNINITIALIZED — no cleanup should happen
    EXPECT_CALL(*driver_hal, wifi_deinit()).Times(0);
    EXPECT_CALL(*driver_hal, event_handler_instance_unregister(_, _, _)).Times(0);

    EXPECT_EQ(ESP_OK, manager->deinit());
}

TEST_F(WiFiManagerInitTest, DeinitAfterInitUnregistersEventHandlers)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());

    // Both handlers must be unregistered during deinit
    EXPECT_CALL(*driver_hal, event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _)).Times(1);
    EXPECT_CALL(*driver_hal, event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, _)).Times(1);

    manager->deinit();
}

TEST_F(WiFiManagerInitTest, DeinitAfterInitCallsWifiDeinit)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());

    EXPECT_CALL(*driver_hal, wifi_deinit()).WillOnce(Return(ESP_OK));

    manager->deinit();
}

TEST_F(WiFiManagerInitTest, DeinitTransitionsToUninitialized)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());
    ASSERT_EQ(State::INITIALIZED, manager->get_state());

    manager->deinit();

    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());
}

TEST_F(WiFiManagerInitTest, DeinitIsIdempotent)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, manager->init());

    manager->deinit();
    EXPECT_EQ(State::UNINITIALIZED, manager->get_state());

    // Second deinit must return ESP_OK without calling wifi_deinit again
    EXPECT_CALL(*driver_hal, wifi_deinit()).Times(0);
    EXPECT_EQ(ESP_OK, manager->deinit());
}