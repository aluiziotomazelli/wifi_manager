#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi_config_storage.hpp"
#include "wifi_driver_hal.hpp"
#include <unity.h>
#include <string>
#include <cstring>

// Mock headers
extern "C" {
#include "Mockesp_wifi.h"
#include "Mockesp_netif.h"
#include "Mockesp_event.h"

// Manual mocks for Wi-Fi specific Netif functions that are not covered by CMock
esp_netif_t* esp_netif_create_default_wifi_sta(void) { return (esp_netif_t*)0x1234; }
void esp_netif_destroy_default_wifi(void *esp_netif) { }
}

// Global state for Wi-Fi config stub
static wifi_config_t s_wifi_config;

esp_err_t my_esp_wifi_set_config(wifi_interface_t interface, wifi_config_t* conf, int cmock_num_calls) {
    if (conf) {
        memcpy(&s_wifi_config, conf, sizeof(wifi_config_t));
    }
    return ESP_OK;
}

esp_err_t my_esp_wifi_get_config(wifi_interface_t interface, wifi_config_t* conf, int cmock_num_calls) {
    if (conf) {
        memcpy(conf, &s_wifi_config, sizeof(wifi_config_t));
    }
    return ESP_OK;
}

esp_err_t my_esp_wifi_restore(int cmock_num_calls) {
    memset(&s_wifi_config, 0, sizeof(wifi_config_t));
    return ESP_OK;
}

void setUp(void)
{
    memset(&s_wifi_config, 0, sizeof(wifi_config_t));

    // Set default behavior for mocks to return ESP_OK and ignore calls
    esp_wifi_init_IgnoreAndReturn(ESP_OK);
    esp_wifi_set_mode_IgnoreAndReturn(ESP_OK);

    // Use stubs for config storage
    esp_wifi_set_config_Stub(my_esp_wifi_set_config);
    esp_wifi_get_config_Stub(my_esp_wifi_get_config);
    esp_wifi_restore_Stub(my_esp_wifi_restore);

    esp_wifi_start_IgnoreAndReturn(ESP_OK);
    esp_wifi_stop_IgnoreAndReturn(ESP_OK);
    esp_wifi_connect_IgnoreAndReturn(ESP_OK);
    esp_wifi_disconnect_IgnoreAndReturn(ESP_OK);
    esp_wifi_deinit_IgnoreAndReturn(ESP_OK);

    esp_netif_init_IgnoreAndReturn(ESP_OK);
    esp_netif_get_handle_from_ifkey_IgnoreAndReturn(NULL);

    esp_event_loop_create_default_IgnoreAndReturn(ESP_OK);
    esp_event_handler_instance_register_IgnoreAndReturn(ESP_OK);
    esp_event_handler_instance_unregister_IgnoreAndReturn(ESP_OK);
}

void tearDown(void)
{
}

TEST_CASE("WiFiConfigStorage basic initialization", "[storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    nvs_flash_erase();
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());
    TEST_ASSERT_EQUAL(ESP_OK, storage.init());

    nvs_flash_deinit();
}

TEST_CASE("WiFiConfigStorage credentials save and load", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("WiFiConfigStorage clear and valid flag", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("WiFiConfigStorage factory reset", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    nvs_flash_erase();
    nvs_flash_init();

    hal.init_wifi();

    storage.init();
    storage.save_valid_flag(true);

    TEST_ASSERT_EQUAL(ESP_OK, storage.factory_reset());
    TEST_ASSERT_FALSE(storage.is_valid());

    hal.deinit();
    nvs_flash_deinit();
}

TEST_CASE("WiFiConfigStorage fallback to Kconfig", "[config_storage]")
{
    WiFiDriverHAL hal;
    WiFiConfigStorage storage(hal, "test_wifi");

    nvs_flash_erase();
    nvs_flash_init();

    hal.init_wifi();
    storage.init();

    TEST_ASSERT_EQUAL(ESP_OK, storage.ensure_config_fallback());
    TEST_ASSERT_TRUE(storage.is_valid());

    std::string loaded_ssid, loaded_pass;
    storage.load_credentials(loaded_ssid, loaded_pass);
    TEST_ASSERT_EQUAL_STRING(CONFIG_WIFI_SSID, loaded_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(CONFIG_WIFI_PASSWORD, loaded_pass.c_str());

    hal.deinit();
    nvs_flash_deinit();
}
