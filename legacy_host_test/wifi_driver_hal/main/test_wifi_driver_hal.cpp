#include "nvs_flash.h"
#include "unity.h"
#include "wifi_driver_hal.hpp"
#include "host_test_common.hpp"

void setUp(void)
{
    host_test_setup_common_mocks();
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
