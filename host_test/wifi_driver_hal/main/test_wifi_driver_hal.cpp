#include "nvs_flash.h"
#include "unity.h"
#include "wifi_driver_hal.hpp"

// Mock headers
extern "C" {
#include "Mockesp_wifi.h"
#include "Mockesp_netif.h"
#include "Mockesp_event.h"

// Manual mocks for functions not covered or needing special behavior
esp_netif_t* esp_netif_create_default_wifi_sta(void) { return (esp_netif_t*)0x1234; }
void esp_netif_destroy_default_wifi(void *esp_netif) { }
}

void setUp(void)
{
    // Set default behavior for mocks to return ESP_OK and ignore calls
    esp_wifi_init_IgnoreAndReturn(ESP_OK);
    esp_wifi_set_mode_IgnoreAndReturn(ESP_OK);
    esp_wifi_set_config_IgnoreAndReturn(ESP_OK);
    esp_wifi_get_config_IgnoreAndReturn(ESP_OK);
    esp_wifi_start_IgnoreAndReturn(ESP_OK);
    esp_wifi_stop_IgnoreAndReturn(ESP_OK);
    esp_wifi_connect_IgnoreAndReturn(ESP_OK);
    esp_wifi_disconnect_IgnoreAndReturn(ESP_OK);
    esp_wifi_restore_IgnoreAndReturn(ESP_OK);
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

TEST_CASE("WiFiDriverHAL: Initialization Sequence", "[driver]")
{
    // NVS is required for WiFi
    nvs_flash_erase();
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());

    WiFiDriverHAL driver;

    // 1. Init Netif
    TEST_ASSERT_EQUAL(ESP_OK, driver.init_netif());

    // 2. Create Event Loop
    TEST_ASSERT_EQUAL(ESP_OK, driver.create_default_event_loop());

    // 3. Setup STA Netif
    TEST_ASSERT_EQUAL(ESP_OK, driver.setup_sta_netif());

    // 4. Init WiFi
    TEST_ASSERT_EQUAL(ESP_OK, driver.init_wifi());

    // 5. Deinit (Cleanup)
    driver.deinit();
    nvs_flash_deinit();
}

TEST_CASE("WiFiDriverHAL: Set Mode and Start/Stop", "[driver]")
{
    nvs_flash_erase();
    nvs_flash_init();

    WiFiDriverHAL driver;
    driver.init_netif();
    driver.create_default_event_loop();
    driver.setup_sta_netif();
    driver.init_wifi();

    // Set Mode
    TEST_ASSERT_EQUAL(ESP_OK, driver.set_mode_sta());

    // Start
    TEST_ASSERT_EQUAL(ESP_OK, driver.start());

    // Stop
    TEST_ASSERT_EQUAL(ESP_OK, driver.stop());

    driver.deinit();
    nvs_flash_deinit();
}
