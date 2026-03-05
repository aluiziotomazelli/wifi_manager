#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi_config_storage.hpp"
#include "wifi_driver_hal.hpp"
#include <unity.h>

// Mock Kconfig if needed, but in idf.py build it should be available via sdkconfig.h

TEST_CASE("WiFiConfigStorage basic initialization", "[storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());
    TEST_ASSERT_EQUAL(ESP_OK, storage.init());
}

TEST_CASE("WiFiConfigStorage credentials save and load", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    hal.init_netif();
    hal.create_default_event_loop();
    hal.setup_sta_netif();
    hal.init_wifi();
    hal.set_mode_sta();

    storage.init();

    std::string ssid = "test_ssid";
    std::string pass = "test_pass";

    TEST_ASSERT_EQUAL(ESP_OK, storage.save_credentials(ssid, pass));
    TEST_ASSERT_TRUE(storage.is_valid());

    std::string loaded_ssid, loaded_pass;
    TEST_ASSERT_EQUAL(ESP_OK, storage.load_credentials(loaded_ssid, loaded_pass));
    TEST_ASSERT_EQUAL_STRING(ssid.c_str(), loaded_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(pass.c_str(), loaded_pass.c_str());

    hal.deinit();
}

TEST_CASE("WiFiConfigStorage clear and valid flag", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    hal.init_wifi();
    hal.set_mode_sta();

    storage.init();

    storage.save_valid_flag(true);
    TEST_ASSERT_TRUE(storage.is_valid());

    storage.save_credentials("test", "test");
    TEST_ASSERT_TRUE(storage.is_valid());

    TEST_ASSERT_EQUAL(ESP_OK, storage.clear_credentials());
    TEST_ASSERT_FALSE(storage.is_valid());

    std::string ssid, pass;
    storage.load_credentials(ssid, pass);
    TEST_ASSERT_EQUAL(0, ssid.length());

    hal.deinit();
}

TEST_CASE("WiFiConfigStorage factory reset", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    hal.init_wifi();

    storage.init();
    storage.save_valid_flag(true);

    TEST_ASSERT_EQUAL(ESP_OK, storage.factory_reset());
    TEST_ASSERT_FALSE(storage.is_valid());

    hal.deinit();
}
