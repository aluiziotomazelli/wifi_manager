// host_test/test_config_storage/main/test_config_storage.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"

using namespace wifi_manager;
using namespace testing;

class MockWiFiDriverHAL : public IWiFiDriverHAL
{
public:
    MOCK_METHOD(esp_err_t, netif_init, (), (override));
    MOCK_METHOD(esp_err_t, event_loop_create_default, (), (override));
    MOCK_METHOD(esp_netif_t *, netif_create_default_wifi_sta, (), (override));
    MOCK_METHOD(esp_netif_t *, netif_get_handle_from_ifkey, (const char *if_key), (override));
    MOCK_METHOD(esp_err_t, wifi_init, (wifi_init_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_set_mode, (wifi_mode_t mode), (override));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_register,
        (esp_event_base_t event_base,
         int32_t event_id,
         esp_event_handler_t event_handler,
         void *handler_arg,
         esp_event_handler_instance_t *instance),
        (override));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_unregister,
        (esp_event_base_t event_base, int32_t event_id, esp_event_handler_instance_t instance),
        (override));
    MOCK_METHOD(esp_err_t, wifi_start, (), (override));
    MOCK_METHOD(esp_err_t, wifi_stop, (), (override));
    MOCK_METHOD(esp_err_t, wifi_connect, (), (override));
    MOCK_METHOD(esp_err_t, wifi_disconnect, (), (override));
    MOCK_METHOD(esp_err_t, wifi_restore, (), (override));
    MOCK_METHOD(esp_err_t, wifi_set_config, (wifi_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_get_config, (wifi_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_deinit, (), (override));
    MOCK_METHOD(void, netif_destroy_default_wifi, (esp_netif_t * netif), (override));
    MOCK_METHOD(
        BaseType_t,
        task_create,
        (TaskFunction_t pvTaskCode,
         const char *const pcName,
         const uint32_t usStackDepth,
         void *const pvParameters,
         UBaseType_t uxPriority,
         TaskHandle_t *const pxCreatedTask),
        (override));
    MOCK_METHOD(void, task_delete, (TaskHandle_t xTaskToDelete), (override));
};

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

TEST_F(WiFiConfigStorageTest, InitPersistsValidFlag)
{
    EXPECT_EQ(ESP_OK, storage.init());

    // Manually save valid flag, then re-init to verify it is loaded from NVS
    storage.save_valid_flag(true);

    WiFiConfigStorage storage2{hal, "test_ns"};
    EXPECT_EQ(ESP_OK, storage2.init());
    EXPECT_TRUE(storage2.is_valid());
}

// =============================================================================
// save_credentials / load_credentials
// =============================================================================

TEST_F(WiFiConfigStorageTest, SaveAndLoadCredentials)
{
    storage.init();

    wifi_config_t saved_cfg = {};

    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&saved_cfg), Return(ESP_OK)));

    // Use a lambda so saved_cfg is read after set_config has already filled it
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce([&saved_cfg](wifi_config_t *cfg) {
        *cfg = saved_cfg;
        return ESP_OK;
    });

    EXPECT_EQ(ESP_OK, storage.save_credentials("myssid", "mypassword"));

    std::string ssid, password;
    EXPECT_EQ(ESP_OK, storage.load_credentials(ssid, password));
    EXPECT_EQ("myssid", ssid);
    EXPECT_EQ("mypassword", password);
}

TEST_F(WiFiConfigStorageTest, ClearCredentialsZeroesSSIDAndPassword)
{
    storage.init();

    wifi_config_t cleared_cfg = {};
    // Capture the config passed to set_config after clearing
    EXPECT_CALL(hal, wifi_get_config(_)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(hal, wifi_set_config(_)).WillRepeatedly(DoAll(SaveArgPointee<0>(&cleared_cfg), Return(ESP_OK)));

    // Save credentials first to ensure there's something to clear
    storage.save_credentials("myssid", "mypassword");

    EXPECT_EQ(ESP_OK, storage.clear_credentials());
    EXPECT_EQ(0, cleared_cfg.sta.ssid[0]);
    EXPECT_EQ(0, cleared_cfg.sta.password[0]);
}

TEST_F(WiFiConfigStorageTest, ClearCredentialsGetConfigFailStillClears)
{
    storage.init();

    wifi_config_t cleared_cfg = {};
    // Even if get_config fails, set_config must still be called with a zeroed config
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce(Return(ESP_FAIL));
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&cleared_cfg), Return(ESP_OK)));

    EXPECT_EQ(ESP_OK, storage.clear_credentials());
    EXPECT_EQ(0, cleared_cfg.sta.ssid[0]);
    EXPECT_EQ(0, cleared_cfg.sta.password[0]);
}

TEST_F(WiFiConfigStorageTest, SaveEdgeCaseSsidAndPassword)
{
    storage.init();

    wifi_config_t saved_cfg = {};

    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&saved_cfg), Return(ESP_OK)));

    // Use a lambda so saved_cfg is read after set_config has already filled it
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce([&saved_cfg](wifi_config_t *cfg) {
        *cfg = saved_cfg;
        return ESP_OK;
    });

    const std::string ssid = std::string(32, 'S');     // 32 chars ssid (max length)
    const std::string password = std::string(64, 'P'); // 64 chars password (max length)

    EXPECT_EQ(ESP_OK, storage.save_credentials(ssid, password));

    std::string loaded_ssid, loaded_password;

    EXPECT_EQ(ESP_OK, storage.load_credentials(loaded_ssid, loaded_password));
    EXPECT_EQ(ssid, loaded_ssid);
    EXPECT_EQ(password, loaded_password);
}

TEST_F(WiFiConfigStorageTest, SaveInvalidSsidAndPasswordTooLong)
{
    storage.init();

    const std::string ssid = std::string(33, 'S');     // 33 chars ssid (more than max length)
    const std::string password = std::string(65, 'P'); // 65 chars password (more than max length)

    EXPECT_EQ(ESP_ERR_INVALID_ARG, storage.save_credentials(ssid, password)); // should not save invalid credentials
    EXPECT_FALSE(storage.is_valid());                                         // must remain false
}

TEST_F(WiFiConfigStorageTest, SaveValidSsidAndInvalidPassword)
{
    storage.init();

    const std::string ssid = std::string(32, 'S');     // 32 char valid ssid
    const std::string password = std::string(65, 'P'); // 65 char invalid password

    EXPECT_EQ(ESP_ERR_INVALID_ARG, storage.save_credentials(ssid, password));
    EXPECT_FALSE(storage.is_valid()); // must remain false
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
    storage.save_credentials("ssid", "pass");
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

// =============================================================================
// ensure_config_fallback
// =============================================================================

TEST_F(WiFiConfigStorageTest, EnsureConfigFallbackSsidPresentValidFlagAlreadySet)
{
    storage.init();

    wifi_config_t cfg = {};
    strncpy((char *)cfg.sta.ssid, "existing_ssid", sizeof(cfg.sta.ssid));
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce(DoAll(SetArgPointee<0>(cfg), Return(ESP_OK)));

    // Manually set valid so no update is needed
    storage.save_valid_flag(true);

    // set_config must NOT be called — nothing to update
    EXPECT_CALL(hal, wifi_set_config(_)).Times(0);
    EXPECT_EQ(ESP_OK, storage.ensure_config_fallback());
}

TEST_F(WiFiConfigStorageTest, EnsureConfigFallbackSsidPresentValidFlagNotSet)
{
    storage.init();

    wifi_config_t cfg = {};
    strncpy((char *)cfg.sta.ssid, "existing_ssid", sizeof(cfg.sta.ssid));
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce(DoAll(SetArgPointee<0>(cfg), Return(ESP_OK)));

    // Driver has SSID but valid flag was never set — must promote to valid
    EXPECT_CALL(hal, wifi_set_config(_)).Times(0);
    EXPECT_EQ(ESP_OK, storage.ensure_config_fallback());
    EXPECT_TRUE(storage.is_valid());
}

TEST_F(WiFiConfigStorageTest, EnsureConfigFallbackGetConfigFails)
{
    storage.init();

    // If get_config fails, ensure_config_fallback must propagate the error
    EXPECT_CALL(hal, wifi_get_config(_)).WillOnce(Return(ESP_FAIL));
    EXPECT_EQ(ESP_FAIL, storage.ensure_config_fallback());
}

TEST_F(WiFiConfigStorageTest, EnsureConfigFallbackToKconfigSsidAndPassword)
{
    storage.init();

    wifi_config_t saved_cfg = {};

    // get_config returns empty SSID, triggering the Kconfig fallback path
    EXPECT_CALL(hal, wifi_get_config(_))
        .WillOnce([&saved_cfg](wifi_config_t *cfg) {
            *cfg = {}; // empty SSID triggers fallback
            return ESP_OK;
        })
        // second call (load_credentials) returns what set_config received
        .WillOnce([&saved_cfg](wifi_config_t *cfg) {
            *cfg = saved_cfg;
            return ESP_OK;
        });

    // capture what ensure_config_fallback passes to set_config
    EXPECT_CALL(hal, wifi_set_config(_)).WillOnce(DoAll(SaveArgPointee<0>(&saved_cfg), Return(ESP_OK)));

    // No ssid in the driver and valid flag is false
    storage.save_valid_flag(false);
    EXPECT_EQ(ESP_OK, storage.ensure_config_fallback());
    EXPECT_TRUE(storage.is_valid());

    // Verify that the credentials are set to Kconfig values
    std::string ssid, password;
    storage.load_credentials(ssid, password);
    EXPECT_STREQ(CONFIG_WIFI_SSID, ssid.c_str());
    EXPECT_STREQ(CONFIG_WIFI_PASSWORD, password.c_str());
}