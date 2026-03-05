#include "esp_err.h"
#include "nvs_flash.h"
#include "unity.h"
#include "wifi_driver_hal.hpp"

TEST_CASE("WiFiDriverHAL: Initialization Sequence", "[driver]")
{
    // NVS is required for WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);

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
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        nvs_flash_erase();
        nvs_flash_init();
    }

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
