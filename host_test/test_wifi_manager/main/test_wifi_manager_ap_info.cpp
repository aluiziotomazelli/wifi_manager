// host_test/test_wifi_manager/main/test_wifi_manager_ap_info.cpp

#include <cstring>
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

class WiFiManagerApInfoTest : public ::testing::Test
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

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::move(sync_manager_owned),
            std::move(state_machine_owned),
            std::move(bootstrapper_owned),
            std::move(processor_owned));
    }

    void TearDown() override { current_state = State::INITIALIZED; }
};

TEST_F(WiFiManagerApInfoTest, GetApInfoWhenUninitializedReturnsInvalidState)
{
    current_state = State::UNINITIALIZED;
    wifi_ap_record_t ap_info = {};

    EXPECT_EQ(ESP_ERR_INVALID_STATE, manager->get_ap_info(ap_info));
}

TEST_F(WiFiManagerApInfoTest, GetApInfoWhenNotConnectedPropagatesDriverError)
{
    current_state = State::STARTED;
    wifi_ap_record_t ap_info = {};

    EXPECT_CALL(*driver_hal, wifi_sta_get_ap_info(_))
        .WillOnce(Return(ESP_ERR_WIFI_NOT_CONNECT));

    EXPECT_EQ(ESP_ERR_WIFI_NOT_CONNECT, manager->get_ap_info(ap_info));
}

TEST_F(WiFiManagerApInfoTest, GetApInfoWhenConnectedPopulatesRecordAndReturnsOk)
{
    current_state = State::CONNECTED_GOT_IP;
    wifi_ap_record_t expected_info = {};
    std::memcpy(expected_info.ssid, "TestSSID", 8);
    expected_info.rssi = -65;

    EXPECT_CALL(*driver_hal, wifi_sta_get_ap_info(_))
        .WillOnce(DoAll(SetArgPointee<0>(expected_info), Return(ESP_OK)));

    wifi_ap_record_t ap_info = {};
    EXPECT_EQ(ESP_OK, manager->get_ap_info(ap_info));
    EXPECT_EQ(std::string(reinterpret_cast<char *>(ap_info.ssid)), "TestSSID");
    EXPECT_EQ(ap_info.rssi, -65);
}

TEST_F(WiFiManagerApInfoTest, GetRssiWhenApInfoFailsReturnsError)
{
    current_state = State::STARTED;
    int8_t rssi = 0;

    EXPECT_CALL(*driver_hal, wifi_sta_get_ap_info(_))
        .WillOnce(Return(ESP_ERR_WIFI_NOT_CONNECT));

    EXPECT_EQ(ESP_ERR_WIFI_NOT_CONNECT, manager->get_rssi(rssi));
    EXPECT_EQ(rssi, 0);
}

TEST_F(WiFiManagerApInfoTest, GetRssiWhenApInfoSucceedsExtractsRssiValue)
{
    current_state = State::CONNECTED_GOT_IP;
    wifi_ap_record_t expected_info = {};
    expected_info.rssi = -42;

    EXPECT_CALL(*driver_hal, wifi_sta_get_ap_info(_))
        .WillOnce(DoAll(SetArgPointee<0>(expected_info), Return(ESP_OK)));

    int8_t rssi = 0;
    EXPECT_EQ(ESP_OK, manager->get_rssi(rssi));
    EXPECT_EQ(rssi, -42);
}
