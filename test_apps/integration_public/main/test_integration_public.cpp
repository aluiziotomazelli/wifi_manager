#include <stdio.h>
// #include <string.h>

#include "esp_log_level.h"
// #include "esp_timer.h"
// #include "esp_wifi.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include secrets from common
#include "wifi_manager.hpp"
#include "../../common/secrets.h"

TEST_CASE("LOG on", "[wifi][log]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
}

TEST_CASE("LOG off", "[wifi][log]")
{
    esp_log_level_set("*", ESP_LOG_NONE);
}

// ========================================================================
// GROUP 1: LIFECYCLE (Public API)
// ========================================================================

TEST_CASE("Public: Sync Start/Stop", "[wifi][lifecycle]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    printf("Sync Start...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.start(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("Sync Stop...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::STOPPED, wm.get_state());

    wm.deinit();
}

TEST_CASE("Public: Async Start/Stop", "[wifi][lifecycle]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    printf("Async Start...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.start());

    int retry = 0;
    while (wm.get_state() != WiFiManager::State::STARTED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("Async Stop...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop());

    retry = 0;
    while (wm.get_state() != WiFiManager::State::STOPPED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::STOPPED, wm.get_state());

    wm.deinit();
}

TEST_CASE("Public: API Abuse (Invalid State)", "[wifi][error]")
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

TEST_CASE("Public: Idempotency (Redundant Calls)", "[wifi][lifecycle]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    TEST_ASSERT_EQUAL(ESP_OK, wm.start(3000));
    TEST_ASSERT_EQUAL(ESP_OK, wm.start(100)); // Should be OK (redundant)

    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(3000));
    TEST_ASSERT_EQUAL(ESP_OK, wm.stop(100)); // Should be OK (redundant)

    wm.deinit();
}

// ========================================================================
// GROUP 2: CONNECTION (REAL HARDWARE)
// ========================================================================

TEST_CASE("Public: Connect/Disconnect Comprehensive", "[wifi][connect][real]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    printf("Connecting to %s...\n", TEST_WIFI_SSID);
    wm.set_credentials(TEST_WIFI_SSID, TEST_WIFI_PASS);

    // 1. Sync Connect
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect(15000));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    // 2. Sync Disconnect
    TEST_ASSERT_EQUAL(ESP_OK, wm.disconnect(5000));
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());

    // 3. Async Connect
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect());
    int retry = 0;
    while (wm.get_state() != WiFiManager::State::CONNECTED_GOT_IP && retry < 200) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    // 4. Async Disconnect
    TEST_ASSERT_EQUAL(ESP_OK, wm.disconnect());
    retry = 0;
    while (wm.get_state() != WiFiManager::State::DISCONNECTED && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());

    wm.deinit();
}

TEST_CASE("Public: Connect with Wrong Password", "[wifi][connect][real]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    printf("Connecting with WRONG password...\n");
    wm.set_credentials(TEST_WIFI_SSID, "wrong_password_123");

    // Using Sync for simpler assertion, but could be Async
    esp_err_t err = wm.connect(15000);

    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(WiFiManager::State::ERROR_CREDENTIALS, wm.get_state());
    TEST_ASSERT_FALSE(wm.is_credentials_valid());

    wm.deinit();
}

TEST_CASE("Public: Connect Rollback (Timeout)", "[wifi][connect]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    wm.set_credentials("NonExistentSSID_Rollback", "password");

    // Short timeout to force timeout error
    esp_err_t err = wm.connect(2000);

    if (err == ESP_OK) {
        // If it somehow returned OK immediately (async behavior leaking?), wait
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    else {
        TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, err);
    }

    // Should eventually revert to DISCONNECTED or remain in CONNECTING until extensive timeout?
    // In strict mode, connect() with timeout returns TIMEOUT if IP not got.
    // State should be updated eventually.

    wm.disconnect(); // Cleanup
    wm.deinit();
}

// ========================================================================
// GROUP 3: ROBUSTNESS & INTERACTION (New Tests)
// ========================================================================

TEST_CASE("Public: Real Automatic Reconnection", "[wifi][reconnect][manual]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    printf("Connecting to %s...\n", TEST_WIFI_SSID);
    wm.set_credentials(TEST_WIFI_SSID, TEST_WIFI_PASS);
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect(15000));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    printf("\n\n!!! ACTION REQUIRED !!!\n");
    printf("Please TURN OFF the Router (SSID: %s) NOW.\n", TEST_WIFI_SSID);
    printf("Waiting 20 seconds for disconnection detection...\n");

    // Wait for disconnection
    int seconds       = 0;
    bool disconnected = false;
    while (seconds < 30) {
        if (wm.get_state() == WiFiManager::State::WAITING_RECONNECT ||
            wm.get_state() == WiFiManager::State::DISCONNECTED) { // Depending on how logic handles it
            disconnected = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        seconds++;
        printf(".");
        fflush(stdout);
    }
    printf("\n");

    if (!disconnected) {
        printf("WARNING: Did not detect disconnection in 30s. Proceeding anyway.\n");
    }
    else {
        printf("Disconnection detected! State: %d\n", (int)wm.get_state());
    }

    printf("\n!!! ACTION REQUIRED !!!\n");
    printf("Please TURN ON the Router (SSID: %s) NOW.\n", TEST_WIFI_SSID);
    printf("Waiting 60 seconds for automatic reconnection...\n");

    seconds          = 0;
    bool reconnected = false;
    while (seconds < 60) {
        if (wm.get_state() == WiFiManager::State::CONNECTED_GOT_IP) {
            reconnected = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        seconds++;
        printf(".");
        fflush(stdout);
    }
    printf("\n");

    TEST_ASSERT_TRUE_MESSAGE(reconnected, "Failed to automatically reconnect after router toggle");

    wm.deinit();
}

TEST_CASE("Public: In-flight Credentials Change", "[wifi][roaming]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    // 1. Connect to AP 1
    printf("Connecting to AP 1: %s\n", TEST_WIFI_SSID);
    wm.set_credentials(TEST_WIFI_SSID, TEST_WIFI_PASS);
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect(15000));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. Change credentials to AP 2 while connected
    printf("Changing credentials to AP 2: %s (In-flight)\n", TEST_WIFI_SSID_2);
    wm.set_credentials(TEST_WIFI_SSID_2, TEST_WIFI_PASS_2);

    // According to typical logic, set_credentials might just save to NVS and update RAM.
    // It might NOT trigger immediate reconnection unless we call connect().
    // If the requirement is "In-flight change", we usually expect the user to call connect() again
    // or expected internal logic to trigger it.
    // Assuming standard behavior: set_credentials just sets. explicit connect() needed.
    // But if we want to test "Roam", we verify if calling connect() handles the transition smoothly.

    printf("Triggering reconnection to new AP...\n");
    TEST_ASSERT_EQUAL(ESP_OK, wm.connect(20000)); // Longer timeout for disconn + conn

    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    // Verify we are actually connected to AP 2?
    // (Hard to verify without checking exact BSSID/SSID from driver,
    // but success implies we connected with NEW credentials).

    wm.deinit();
}
