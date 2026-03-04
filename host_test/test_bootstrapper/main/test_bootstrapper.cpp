#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_bootstrapper.hpp"
#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"

using namespace wifi_manager;
using namespace testing;

class WiFiBootstrapperTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> driver_hal;
    NiceMock<MockWiFiConfigStorage> storage;
    NiceMock<MockWiFiSyncManager> sync_manager;
    NiceMock<MockWiFiStateMachine> state_machine;

    std::unique_ptr<WiFiBootstrapper> bootstrapper;
    TaskHandle_t task_handle = nullptr;

    void SetUp() override
    {
        bootstrapper = std::make_unique<WiFiBootstrapper>(driver_hal, storage, sync_manager);
    }

    void setup_successful_init()
    {
        ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, event_loop_create_default()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, netif_get_handle_from_ifkey(_)).WillByDefault(Return(nullptr));
        ON_CALL(driver_hal, netif_create_default_wifi_sta()).WillByDefault(Return(reinterpret_cast<esp_netif_t *>(0x1)));
        ON_CALL(driver_hal, wifi_init(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, wifi_set_mode(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(sync_manager, init()).WillByDefault(Return(ESP_OK));

        // Simulate returning valid pointers for event handler instances
        ON_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _))
            .WillByDefault(DoAll(SetArgPointee<4>(reinterpret_cast<esp_event_handler_instance_t>(0x123)), Return(ESP_OK)));
        ON_CALL(driver_hal, event_handler_instance_register(IP_EVENT, _, _, _, _))
            .WillByDefault(DoAll(SetArgPointee<4>(reinterpret_cast<esp_event_handler_instance_t>(0x456)), Return(ESP_OK)));

        ON_CALL(storage, ensure_config_fallback()).WillByDefault(Return(ESP_OK));
        ON_CALL(driver_hal, task_create(_, _, _, _, _, _)).WillByDefault(Return(pdPASS));
    }
};

TEST_F(WiFiBootstrapperTest, InitSuccess)
{
    setup_successful_init();
    EXPECT_EQ(ESP_OK, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitStorageFailure)
{
    EXPECT_CALL(storage, init()).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitNetifFailure)
{
    ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(driver_hal, netif_init()).WillOnce(Return(ESP_ERR_NO_MEM));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitEventLoopFailure)
{
    ON_CALL(storage, init()).WillByDefault(Return(ESP_OK));
    ON_CALL(driver_hal, netif_init()).WillByDefault(Return(ESP_OK));
    EXPECT_CALL(driver_hal, event_loop_create_default()).WillOnce(Return(ESP_ERR_NO_MEM));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitWifiEventRegisterFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitIpEventRegisterFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, event_handler_instance_register(WIFI_EVENT, _, _, _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(driver_hal, event_handler_instance_register(IP_EVENT, _, _, _, _)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_FAIL, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, InitTaskFailure)
{
    setup_successful_init();
    EXPECT_CALL(driver_hal, task_create(_, _, _, _, _, _)).WillOnce(Return(pdFAIL));
    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_EQ(ESP_ERR_NO_MEM, bootstrapper->init(nullptr, &task_handle));
}

TEST_F(WiFiBootstrapperTest, DeinitUnregistersEventHandlers)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(nullptr, &task_handle));

    // Verify that unregister is called with the handles provided during init
    EXPECT_CALL(driver_hal, event_handler_instance_unregister(WIFI_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x123))).Times(1);
    EXPECT_CALL(driver_hal, event_handler_instance_unregister(IP_EVENT, _, reinterpret_cast<esp_event_handler_instance_t>(0x456))).Times(1);

    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
}

TEST_F(WiFiBootstrapperTest, DeinitSuccess)
{
    setup_successful_init();
    ASSERT_EQ(ESP_OK, bootstrapper->init(nullptr, &task_handle));

    EXPECT_CALL(driver_hal, wifi_deinit()).Times(1);
    EXPECT_CALL(sync_manager, deinit()).Times(1);
    EXPECT_EQ(ESP_OK, bootstrapper->deinit(&task_handle));
}
