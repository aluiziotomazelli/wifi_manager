// test_wifi_manager.cpp
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "unity.h"

#include "secrets.h"
#include "test_memory_helper.h"
#include "wifi_manager.hpp"

#define UNIT_TEST 1
#ifdef UNIT_TEST

extern "C" void test_warmup(void)
{
    printf("\n=== WiFiManager Warmup ===\n");
    printf("Pre-allocating WiFi, NVS and Netif internal buffers...\n");
    WiFiManager &wm = WiFiManager::get_instance();
    wm.init();
    wm.start(5000);
    wm.stop(5000);
    wm.deinit();
    printf("Warmup complete. Memory state stabilized.\n");

    // Default to ERROR for tests, can be changed via specific test cases
    esp_log_level_set("*", ESP_LOG_ERROR);
    printf("Log level set to ERROR for all components.\n");
    printf("==========================\n\n");
}

static void print_memory(const char *label)
{
    size_t free_8bit  = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    printf("%s - 8BIT: %u, 32BIT: %u bytes free\n", label, (unsigned)free_8bit,
           (unsigned)free_32bit);
}

// ========================================================================
// GROUP 1: LOG CONTROLS
// ========================================================================

TEST_CASE("1 LOG on", "[wifi][log]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
}

TEST_CASE("2 LOG off", "[wifi][log]")
{
    esp_log_level_set("*", ESP_LOG_ERROR);
}

// ========================================================================
// GROUP 2: NVS AND CREDENTIALS
// ========================================================================

TEST_CASE("3 test_wifi_init_once", "[wifi][init]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();

    printf("Testing WiFi Manager initialization...\n");
    esp_err_t ret = wm.init();
    TEST_ASSERT(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);

    ret = wm.deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("4 test_wifi_credentials", "[wifi][nvs]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    std::string test_ssid = "TestNetwork";
    std::string test_pass = "TestPassword123";

    printf("Setting credentials: SSID=%s\n", test_ssid.c_str());
    esp_err_t ret = wm.set_credentials(test_ssid, test_pass);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    printf("Getting credentials from Driver...\n");
    std::string read_ssid, read_pass;
    ret = wm.get_credentials(read_ssid, read_pass);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING(test_ssid.c_str(), read_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(test_pass.c_str(), read_pass.c_str());

    wm.deinit();
}

TEST_CASE("5 test_credentials_deep", "[wifi][nvs]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    // SSID: 32 chars, Password: 64 chars
    std::string max_ssid(32, 'S');
    std::string max_pass(64, 'P');

    printf("Testing 32-char SSID and 64-char Password...\n");
    esp_err_t err = wm.set_credentials(max_ssid, max_pass);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    std::string read_ssid, read_pass;
    err = wm.get_credentials(read_ssid, read_pass);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(32, read_ssid.length());
    TEST_ASSERT_EQUAL(64, read_pass.length());
    TEST_ASSERT_EQUAL_STRING(max_ssid.c_str(), read_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(max_pass.c_str(), read_pass.c_str());

    wm.deinit();
    wm.init();
    TEST_ASSERT_TRUE(wm.is_credentials_valid());
    wm.get_credentials(read_ssid, read_pass);
    TEST_ASSERT_EQUAL_STRING(max_ssid.c_str(), read_ssid.c_str());

    wm.deinit();
}

TEST_CASE("6 test_nvs_leak", "[memory][nvs]")
{
    printf("\n=== Testing NVS Memory Leak ===\n");
    print_memory("Before NVS init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    print_memory("After NVS init");
    nvs_flash_deinit();
    print_memory("After NVS deinit");
}

TEST_CASE("7 test_wifi_valid_flag_persistence", "[wifi][nvs]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    nvs_flash_erase();
    wm.init();

    wm.set_credentials("ValidSSID", "ValidPass");
    TEST_ASSERT_TRUE(wm.is_credentials_valid());

    wm.deinit();
    wm.init();
    TEST_ASSERT_TRUE(wm.is_credentials_valid());

    wm.clear_credentials();
    TEST_ASSERT_FALSE(wm.is_credentials_valid());

    wm.deinit();
    wm.init();
#ifdef CONFIG_WIFI_SSID
    if (strlen(CONFIG_WIFI_SSID) > 0) {
        TEST_ASSERT_TRUE(wm.is_credentials_valid());
    }
    else {
        TEST_ASSERT_FALSE(wm.is_credentials_valid());
    }
#else
    TEST_ASSERT_FALSE(wm.is_credentials_valid());
#endif

    wm.deinit();
}

TEST_CASE("8 test_wifi_factory_reset", "[wifi][nvs]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    wm.set_credentials("FactorySSID", "FactoryPass");
    TEST_ASSERT_TRUE(wm.is_credentials_valid());

    printf("Calling factory_reset()...\n");
    esp_err_t err = wm.factory_reset();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FALSE(wm.is_credentials_valid());
    TEST_ASSERT_EQUAL(WiFiManager::State::INITIALIZED, wm.get_state());

    wm.deinit();
}

TEST_CASE("9 test_nvs_auto_repair", "[wifi][nvs]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    nvs_flash_deinit();

    printf("Erasing NVS flash...\n");
    esp_err_t err = nvs_flash_erase();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    printf("Initializing WiFiManager after NVS erase...\n");
    err = wm.init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = wm.set_credentials("RepairSSID", "RepairPass");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    wm.deinit();
}

// ========================================================================
// GROUP 3: LIFECYCLE (INIT/START/STOP)
// ========================================================================

TEST_CASE("10 test_singleton_pattern", "[wifi][singleton]")
{
    WiFiManager &instance1 = WiFiManager::get_instance();
    WiFiManager &instance2 = WiFiManager::get_instance();
    TEST_ASSERT_EQUAL_PTR(&instance1, &instance2);
}

TEST_CASE("11 test_multiple_init_calls", "[wifi][init]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    TEST_ASSERT_EQUAL(ESP_OK, wm.init());
    TEST_ASSERT_EQUAL(ESP_OK, wm.init());
    wm.deinit();
}

TEST_CASE("12 test_state_transitions", "[wifi][state]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManager::State state = wm.get_state();
    TEST_ASSERT(state == WiFiManager::State::INITIALIZED);
    wm.deinit();
}

TEST_CASE("13 test_wifi_start_stop_comprehensive", "[wifi][state]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    printf("1. Sync Start...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.start(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("2. Sync Stop...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::STOPPED, wm.get_state());

    printf("3. Async Start...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.start());
    int retry = 0;
    while (wm.get_state() != WiFiManager::State::STARTED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("4. Async Stop...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop());
    retry = 0;
    while (wm.get_state() != WiFiManager::State::STOPPED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::STOPPED, wm.get_state());

    wm.deinit();
}

TEST_CASE("14 test_wifi_rapid_start_stop", "[wifi][stress]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, wm.start(5000));
        TEST_ASSERT_EQUAL(ESP_OK, wm.stop(5000));
    }
    wm.deinit();
}

TEST_CASE("15 test_wifi_spam_robustness", "[wifi][stress]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start();

    printf("Sending 100 redundant connect commands...\n");
    wm.set_credentials("StressSSID", "password");
    for (int i = 0; i < 100; i++) {
        wm.connect();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    wm.deinit();
}

TEST_CASE("16 test_wifi_api_abuse", "[wifi][error]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wm.start(1000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wm.connect(1000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wm.disconnect(1000));

    wm.init();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wm.connect(1000));
    wm.deinit();
}

TEST_CASE("17 test_start_stop_state_validation", "[wifi][state][startstop]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    printf("Redundant START...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.start(3000));
    TEST_ASSERT_EQUAL(ESP_OK, wm.start(100));

    printf("Redundant STOP...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(3000));
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(100));

    wm.deinit();
}

// ========================================================================
// GROUP 4: CONNECTION (REAL AP)
// ========================================================================

TEST_CASE("18 test_wifi_connect_disconnect_comprehensive", "[wifi][connect][real]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    wm.set_credentials(TEST_WIFI_SSID, TEST_WIFI_PASS);

    printf("1. Synchronous Connect...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect(15000));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    printf("2. Synchronous Disconnect...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.disconnect(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());

    printf("3. Asynchronous Connect...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect());
    int retry = 0;
    while (wm.get_state() != WiFiManager::State::CONNECTED_GOT_IP && retry < 150) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    printf("4. Asynchronous Disconnect...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.disconnect());
    retry = 0;
    while (wm.get_state() != WiFiManager::State::DISCONNECTED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());

    wm.deinit();
}

TEST_CASE("19 test_wifi_connect_wrong_password_sync", "[wifi][connect][real]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    printf("Connecting with WRONG password (Sync)...\n");
    wm.set_credentials(TEST_WIFI_SSID, "wrong_password_123");
    esp_err_t err = wm.connect(15000);

    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(WiFiManager::State::ERROR_CREDENTIALS, wm.get_state());
    TEST_ASSERT_FALSE(wm.is_credentials_valid());
    wm.deinit();
}

TEST_CASE("20 test_wifi_connect_wrong_password_async", "[wifi][connect][real]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    printf("Connecting with WRONG password (Async)...\n");
    wm.set_credentials(TEST_WIFI_SSID, "wrong_password_123");
    wm.connect();

    int retry = 0;
    while (wm.get_state() != WiFiManager::State::ERROR_CREDENTIALS && retry < 150) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }

    TEST_ASSERT_EQUAL(WiFiManager::State::ERROR_CREDENTIALS, wm.get_state());
    TEST_ASSERT_FALSE(wm.is_credentials_valid());
    wm.deinit();
}

TEST_CASE("21 test_wifi_connect_rollback", "[wifi][connect]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    wm.set_credentials("NonExistentSSID_Rollback", "password");
    esp_err_t err = wm.connect(1000);
    printf("Error: %s\n", esp_err_to_name(err));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, err);
    printf("State after connect timeout: %d\n", (int)wm.get_state());

    int retry = 0;
    while (wm.get_state() == WiFiManager::State::CONNECTING && retry < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    printf("State before assert DISCONNECTED: %d\n", (int)wm.get_state());
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());
    wm.deinit();
}

TEST_CASE("22 test_wifi_start_rollback", "[wifi][state]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    esp_err_t err = wm.start(1);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, err);

    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(wm.get_state() == WiFiManager::State::STOPPED);
    wm.deinit();
}

#endif // UNIT_TEST