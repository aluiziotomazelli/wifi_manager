#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"
#include "wifi_bootstrapper.hpp"

using namespace wifi_manager;
using namespace testing;

// Minimal task stub used as the TaskFunction_t argument in init() calls.
// The function is never executed during tests because task_create is mocked;
// it only exists to satisfy the function-pointer parameter type.
static void dummy_task(void *) {}

class WiFiBootstrapperTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> driver_hal;
    NiceMock<MockWiFiConfigStorage> storage;
    NiceMock<MockWiFiSyncManager> sync_manager;
    NiceMock<MockWiFiStateMachine> state_machine;
    // NiceMock<MockWiFiEventHandler> event_handler;

    std::unique_ptr<WiFiBootstrapper> bootstrapper;

    TaskHandle_t task_handle = nullptr;

    void SetUp() override { bootstrapper = std::make_unique<WiFiBootstrapper>(driver_hal, storage, sync_manager); }

    void setup_successful_init()
    {
        ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
        ON_CALL(driver_hal, netif_create_default_wifi_sta())
            .WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));
        ON_CALL(driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(sync_manager, init()).WillByDefault(Return(ESP_OK));

        // Simulate returning valid pointers for event handler instances
        ON_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _))
            .WillByDefault(
                DoAll(SetArgPointee<4>(reinterpret_cast<esp_event_handler_instance_t>(0x123)), Return(ESP_OK)));
        ON_CALL(driver_hal, event_handler_instance_register(IP_EVENT, _, _, _, _))
            .WillByDefault(
                DoAll(SetArgPointee<4>(reinterpret_cast<esp_event_handler_instance_t>(0x456)), Return(ESP_OK)));

        ON_CALL(driver_hal, task_create(_, _, _, _, _, _)).WillByDefault(Return(pdPASS));
    }
};

TEST_F(WiFiBootstrapperTest, InitSuccess)
{
    setup_successful_init();
    EXPECT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitWithExistingNetif)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, netif_get_handle_from_ifkey(_)).WillOnce(Return(reinterpret_cast<esp_netif_t *>(0x1)));
    EXPECT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitStorageFailure)
{
    EXPECT_CALL(storage, init()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitNetifFailure)
{
    ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(driver_hal, netif_init()).WillOnce(Return(ESP_ERR_NO_MEM));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitEventLoopFailure)
{
    ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(driver_hal, event_loop_create_default()).WillOnce(Return(ESP_ERR_NO_MEM));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitWifiEventRegisterFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitIpEventRegisterFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(driver_hal, event_handler_instance_register(IP_EVENT, _, _, _, _)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitTaskFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, task_create(_, _, _, _, _, _)).WillOnce(Return(pdFAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitConfigWifiModeFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, wifi_set_mode(_)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitWithSuccessfulLoadCredentials)
{
    setup_successful_init();
    ON_CALL(storage, is_valid()).WillByDefault(Return(true)); // If valid
    EXPECT_CALL(storage, load_credentials(_, _))
        .Times(1)
        .WillOnce(Return(ESP_OK));                        // load_credentials must be called, if returns ESP_OK
    EXPECT_CALL(storage, add_credentials(_, _)).Times(1); // add_credentials must be called
    EXPECT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle)); // init will not fail due credentials
}

TEST_F(WiFiBootstrapperTest, InitWithFailedLoadCredentials)
{
    setup_successful_init();
    ON_CALL(storage, is_valid()).WillByDefault(Return(true)); // If valid
    ON_CALL(storage, load_credentials(_, _))
        .WillByDefault(Return(ESP_FAIL));                 // load_credentials will be called, if returns ESP_FAIL
    EXPECT_CALL(storage, add_credentials(_, _)).Times(0); // add_credentials must not be called
    EXPECT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle)); // init will not fail due credentials
}

TEST_F(WiFiBootstrapperTest, InitSkipsCredentialSyncWhenNotValid)
{
    setup_successful_init();
    ON_CALL(storage, is_valid()).WillByDefault(Return(false)); // If not valid
    EXPECT_CALL(storage, load_credentials(_, _)).Times(0);     // load_credentials must not be called
    EXPECT_CALL(storage, add_credentials(_, _)).Times(0);      // add_credentials must not be called
    EXPECT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitSyncManagerFailure)
{
    setup_successful_init();
    EXPECT_CALL(sync_manager, init()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitCreateDefaultWifiStaFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, netif_create_default_wifi_sta()).WillOnce(Return(nullptr));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitWifiInitFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, wifi_init(_)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(dummy_task, nullptr, &task_handle));
}

// =====================================================================
// Deinit tests
// =====================================================================

TEST_F(WiFiBootstrapperTest, DeinitUnregistersEventHandlers)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Verify that unregister is called with the handles provided during init
    EXPECT_CALL(
        driver_hal,
        event_handler_instance_unregister(WIFI_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x123)))
        .Times(1);
    EXPECT_CALL(
        driver_hal,
        event_handler_instance_unregister(IP_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x456)))
        .Times(1);

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
}

TEST_F(WiFiBootstrapperTest, DeinitUnregistersEventHandlersFailure)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Verify that unregister is called with the handles provided during init
    EXPECT_CALL(
        driver_hal,
        event_handler_instance_unregister(WIFI_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x123)))
        .Times(1)
        .WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(
        driver_hal,
        event_handler_instance_unregister(IP_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x456)))
        .Times(1)
        .WillOnce(Return(ESP_FAIL));

    EXPECT_EQ(ESP_FAIL, bootstrapper->deinit(&task_handle));
}

TEST_F(WiFiBootstrapperTest, DeinitSuccess)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_CALL(sync_manager, deinit()).Times(1);
    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
}

TEST_F(WiFiBootstrapperTest, DeinitStopsTaskGracefully)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Simulate a non-null task handle
    TaskHandle_t fake_handle = reinterpret_cast<TaskHandle_t>(0x1);
    task_handle = fake_handle;

    // sync_manager is initialized and post_message succeeds
    ON_CALL(sync_manager, is_initialized()).WillByDefault(Return(true));
    ON_CALL(sync_manager, post_message(_))
        .WillByDefault(DoAll(
            // Simulate task clearing its own handle on EXIT
            Assign(&task_handle, nullptr),
            Return(ESP_OK)));

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
    EXPECT_EQ(nullptr, task_handle);
}

TEST_F(WiFiBootstrapperTest, DeinitForcesTaskDeleteWhenNotExiting)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // valid fake task handle - task never clears the handle
    TaskHandle_t fake_handle = reinterpret_cast<TaskHandle_t>(0x1);
    task_handle = fake_handle;

    // post_message returns OK but task never clears the handle - forces task_delete
    ON_CALL(sync_manager, is_initialized()).WillByDefault(Return(true));
    ON_CALL(sync_manager, post_message(_)).WillByDefault(Return(ESP_OK));

    EXPECT_CALL(driver_hal, task_delete(fake_handle)).Times(1);

    bootstrapper->deinit(&task_handle);
    EXPECT_EQ(nullptr, task_handle);
}

TEST_F(WiFiBootstrapperTest, DeinitWithNullptrTaskHandleSkipsTaskStop)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Passing nullptr as pxTaskHandle — deinit must skip the task stop block entirely
    EXPECT_CALL(sync_manager, post_message(_)).Times(0);
    EXPECT_CALL(driver_hal, task_delete(_)).Times(0);

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(nullptr));
}

TEST_F(WiFiBootstrapperTest, DeinitForcesTaskDeleteWhenSyncManagerNotInitialized)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Simulate a non-null task handle
    TaskHandle_t task_handle = reinterpret_cast<TaskHandle_t>(0x1);

    // sync_manager is not initialized
    ON_CALL(sync_manager, is_initialized()).WillByDefault(Return(false));

    // task_delete must be called because sync_manager is not initialized
    EXPECT_CALL(driver_hal, task_delete(_)).Times(1);

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
    EXPECT_EQ(nullptr, task_handle);
}

TEST_F(WiFiBootstrapperTest, DeinitForcesTaskDeleteWhenPostMessageFails)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(dummy_task, nullptr, &task_handle));

    // Simulate a non-null task handle
    TaskHandle_t task_handle = reinterpret_cast<TaskHandle_t>(0x1);

    // sync_manager is initialized and post_message fails
    ON_CALL(sync_manager, is_initialized()).WillByDefault(Return(true));
    ON_CALL(sync_manager, post_message(_)).WillByDefault(Return(ESP_FAIL));

    // task_delete must be called because post_message fails
    EXPECT_CALL(driver_hal, task_delete(_)).Times(1);

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
    EXPECT_EQ(nullptr, task_handle);
}
