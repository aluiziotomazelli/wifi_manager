// host_test/test_config_storage/main/test_config_storage.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nvs_flash.h"
#include "sdkconfig.h"

#include "mock_wifi_driver_hal.hpp"
#include "wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"

using namespace wifi_manager;
using namespace testing;

class WiFiConfigStorageTest : public ::testing::Test
{
protected:
    // NiceMock suppresses warnings for unexpected calls on methods not relevant to storage tests
    NiceMock<MockWiFiDriverHAL> hal;
    WiFiConfigStorage storage{hal, "test_ns"};

    void SetUp() override
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    void TearDown() override { nvs_flash_deinit(); }
};

// Test for real nvs in linux target, a file on the disk used as NVS partition
TEST_F(WiFiConfigStorageTest, NvsInit)
{
    nvs_flash_erase();
    EXPECT_EQ(nvs_flash_init(), ESP_OK);
    nvs_flash_deinit();
}

// =============================================================================
// init
// =============================================================================

TEST_F(WiFiConfigStorageTest, InitLoadsValidFlag)
{
    // First init: no valid flag stored yet, init should still return ESP_OK
    EXPECT_EQ(ESP_OK, storage.init());

    // But is_valid() should return false
    EXPECT_FALSE(storage.is_valid());
}

TEST_F(WiFiConfigStorageTest, InitPersistsValidFlagAndSyncsToDriver)
{
    EXPECT_EQ(ESP_OK, storage.init());

    // Save credentials, which sets valid flag
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(Return(ESP_OK));
    storage.add_credentials("sync_ssid", "sync_pass");

    // Re-init with a new instance
    WiFiConfigStorage storage2{hal, "test_ns"};
    // Init should now load "sync_ssid" and call wifi_set_config on driver
    wifi_config_t synced_cfg = {};
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&synced_cfg), Return(ESP_OK)));

    EXPECT_EQ(ESP_OK, storage2.init());
    EXPECT_TRUE(storage2.is_valid());
    EXPECT_STREQ("sync_ssid", (char *)synced_cfg.sta.ssid);
}

// =============================================================================
// add_credentials / load_credentials
// =============================================================================

TEST_F(WiFiConfigStorageTest, AddAndLoadCredentials)
{
    storage.init();

    wifi_config_t saved_cfg = {};
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&saved_cfg), Return(ESP_OK)));

    EXPECT_EQ(ESP_OK, storage.add_credentials("myssid", "mypassword"));

    std::string ssid, password;
    EXPECT_EQ(ESP_OK, storage.load_credentials(ssid, password));
    EXPECT_EQ("myssid", ssid);
    EXPECT_EQ("mypassword", password);
    EXPECT_STREQ("myssid", (char *)saved_cfg.sta.ssid);
}

TEST_F(WiFiConfigStorageTest, AddMultipleCredentialsIncremental)
{
    storage.init();

    EXPECT_CALL(hal, wifi_set_config(_)).Times(2).WillRepeatedly(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, storage.add_credentials("ssid0", "pass0"));
    EXPECT_EQ(ESP_OK, storage.add_credentials("ssid1", "pass1"));

    // Verify ap_count in NVS
    nvs_handle_t h;
    nvs_open("test_ns", NVS_READONLY, &h);
    uint8_t count = 0;
    nvs_get_u8(h, "ap_count", &count);
    EXPECT_EQ(2, count);
    nvs_close(h);

    // load_credentials should return the last added one
    std::string ssid, pass;
    storage.load_credentials(ssid, pass);
    EXPECT_EQ("ssid1", ssid);
}

TEST_F(WiFiConfigStorageTest, MaxApCount)
{
    storage.init();

    EXPECT_CALL(hal, wifi_set_config(_)).WillRepeatedly(Return(ESP_OK));

    uint8_t count = 0;

    // MAX_AP_COUNT = 10
    for (int i = 1; i <= WiFiConfigStorage::MAX_AP_COUNT + 5; i++) {
        storage.add_credentials("ssid" + std::to_string(i), "pass" + std::to_string(i));

        if (i <= WiFiConfigStorage::MAX_AP_COUNT) {
            // Verify ap_count in NVS
            nvs_handle_t h;
            nvs_open("test_ns", NVS_READONLY, &h);
            nvs_get_u8(h, "ap_count", &count);
            EXPECT_EQ(i, count);
            nvs_close(h);

            // load_credentials to verify if all are saved
            std::string ssid, pass;
            storage.load_credentials(ssid, pass);
            EXPECT_EQ("ssid" + std::to_string(i), ssid);
        }
        else {
            nvs_handle_t h;
            nvs_open("test_ns", NVS_READONLY, &h);
            nvs_get_u8(h, "ap_count", &count);
            EXPECT_EQ(WiFiConfigStorage::MAX_AP_COUNT, count);
            nvs_close(h);

            // load_credentials should return the last added one
            std::string ssid, pass;
            storage.load_credentials(ssid, pass);
            EXPECT_EQ("ssid" + std::to_string(i), ssid); // i = 11, not 10
        }
    }
}

TEST_F(WiFiConfigStorageTest, AddExistingSsidUpdatesInsteadOfAdding)
{
    storage.init();

    EXPECT_CALL(hal, wifi_set_config(_)).Times(2).WillRepeatedly(Return(ESP_OK));

    storage.add_credentials("ssid0", "pass_old");
    storage.add_credentials("ssid0", "pass_new");

    nvs_handle_t h;
    nvs_open("test_ns", NVS_READONLY, &h);
    uint8_t count = 0;
    nvs_get_u8(h, "ap_count", &count);
    EXPECT_EQ(1, count); // Should still be 1
    nvs_close(h);

    std::string ssid, pass;
    storage.load_credentials(ssid, pass);
    EXPECT_EQ("pass_new", pass);
}

TEST_F(WiFiConfigStorageTest, ClearCredentialsErasesAllAndDriver)
{
    storage.init();

    EXPECT_CALL(hal, wifi_set_config(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    storage.add_credentials("ssid0", "pass0");

    EXPECT_EQ(ESP_OK, storage.clear_credentials());
    EXPECT_FALSE(storage.is_valid());

    // Verify NVS is empty by trying to load (should fail to open or find keys)
    std::string ssid, pass;
    EXPECT_NE(ESP_OK, storage.load_credentials(ssid, pass));
}

TEST_F(WiFiConfigStorageTest, SaveEdgeCaseSsidAndPassword)
{
    storage.init();

    wifi_config_t saved_cfg = {};
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&saved_cfg), Return(ESP_OK)));

    const std::string ssid = std::string(32, 'S');     // 32 chars ssid (max length)
    const std::string password = std::string(64, 'P'); // 64 chars password (max length)

    EXPECT_EQ(ESP_OK, storage.add_credentials(ssid, password));

    std::string loaded_ssid, loaded_password;
    EXPECT_EQ(ESP_OK, storage.load_credentials(loaded_ssid, loaded_password));
    EXPECT_EQ(ssid, loaded_ssid);
    EXPECT_EQ(password, loaded_password);
}

TEST_F(WiFiConfigStorageTest, AddInvalidSsidAndPasswordTooLong)
{
    storage.init();

    const std::string ssid = std::string(33, 'S');     // 33 chars ssid (more than max length)
    const std::string password = std::string(65, 'P'); // 65 chars password (more than max length)

    EXPECT_EQ(ESP_ERR_INVALID_ARG, storage.add_credentials(ssid, password)); // should not save invalid credentials
    EXPECT_FALSE(storage.is_valid());                                        // must remain false
}

// =============================================================================
// factory_reset
// =============================================================================

TEST_F(WiFiConfigStorageTest, FactoryResetCallsWifiRestore)
{
    storage.init();
    EXPECT_FALSE(storage.is_valid());

    // Save valid flag to ensure there's something to reset
    storage.save_valid_flag(true);
    EXPECT_TRUE(storage.is_valid());

    // factory_reset must call wifi_restore to clear driver-level config
    EXPECT_CALL(hal, wifi_restore()).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_OK, storage.factory_reset());
    EXPECT_FALSE(storage.is_valid());
}

TEST_F(WiFiConfigStorageTest, FactoryResetSetsValidFalse)
{
    storage.init();
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(Return(ESP_OK));
    storage.add_credentials("ssid", "pass");
    EXPECT_TRUE(storage.is_valid());

    EXPECT_CALL(hal, wifi_restore()).WillOnce(Return(ESP_OK));
    storage.factory_reset();

    // After factory reset, valid flag must be cleared both in memory and NVS
    EXPECT_FALSE(storage.is_valid());

    // Re-init to verify NVS was also erased
    WiFiConfigStorage storage2{hal, "test_ns"};
    storage2.init();
    EXPECT_FALSE(storage2.is_valid());
}