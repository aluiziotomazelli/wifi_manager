// components/wifi_manager/test/main/test_wifi_manager_internal.cpp
#include <stdio.h>

#include "esp_timer.h"
#include "esp_wifi.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "test_memory_helper.h"
#include "test_wifi_manager_accessor.hpp"

// ========================================================================
// GROUP 5: INTERNAL SIMULATION
// These tests use the TestAccessor to simulate driver events and verify
// the state machine logic without requiring a real Access Point.
// ========================================================================

#define UNIT_TEST 1
#ifdef UNIT_TEST

TEST_CASE("23 test_internal_queue_behavior", "[wifi][internal][stress]")
{
    set_memory_leak_threshold(-2000);
    printf("\n=== Test: Queue Behaviors ===\n");

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    WiFiManagerTestAccessor accessor(wm);

    const int COMMANDS_TO_SEND = 15;
    int successful_sends       = 0;
    for (int i = 0; i < COMMANDS_TO_SEND; i++) {
        if (accessor.test_send_start_command(true) == ESP_OK) {
            successful_sends++;
        }
    }

    TEST_ASSERT_EQUAL(COMMANDS_TO_SEND, successful_sends);
    vTaskDelay(pdMS_TO_TICKS(100));
    wm.deinit();
}

TEST_CASE("24 test_internal_connection_flow", "[wifi][internal][state]")
{
    set_memory_leak_threshold(-2000);
    printf("\n=== Test: Connection Flow Simulation ===\n");

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    printf("Starting WiFi...\n");
    wm.start(); // Async
    vTaskDelay(pdMS_TO_TICKS(1));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTING, wm.get_state());

    printf("Simulating WIFI_EVENT_STA_START...\n");
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("Connecting...\n");
    wm.connect(); // Async
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTING, wm.get_state());

    printf("Simulating WIFI_EVENT_STA_CONNECTED...\n");
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_NO_IP, wm.get_state());

    printf("Simulating IP_EVENT_STA_GOT_IP...\n");
    accessor.test_simulate_ip_event(IP_EVENT_STA_GOT_IP);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    wm.deinit();
}

TEST_CASE("25 test_internal_auto_reconnect", "[wifi][internal][reconnect]")
{
    set_memory_leak_threshold(-2000);
    printf("\n=== Test: Auto-Reconnect Simulation ===\n");

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    wm.set_credentials("ReconnectSSID", "pass");

    accessor.test_send_connect_command(false);
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_CONNECTED);
    accessor.test_simulate_ip_event(IP_EVENT_STA_GOT_IP);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    printf("Simulating Beacon Timeout...\n");
    accessor.test_simulate_disconnect(WIFI_REASON_BEACON_TIMEOUT);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());

    wm.deinit();
}

TEST_CASE("26 test_internal_immediate_invalidation", "[wifi][internal][reconnect]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("InvalidPassSSID", "wrong");
    accessor.test_simulate_disconnect(WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT);
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(WiFiManager::State::ERROR_CREDENTIALS, wm.get_state());
    TEST_ASSERT_FALSE(wm.is_credentials_valid());

    wm.deinit();
}

TEST_CASE("27 test_internal_3_strikes", "[wifi][internal][reconnect]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("SuspectSSID", "pass");

    for (int i = 1; i <= 2; i++) {
        accessor.test_simulate_disconnect(WIFI_REASON_CONNECTION_FAIL);
        vTaskDelay(pdMS_TO_TICKS(100));
        TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());
    }

    accessor.test_simulate_disconnect(WIFI_REASON_CONNECTION_FAIL);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::ERROR_CREDENTIALS, wm.get_state());

    wm.deinit();
}

TEST_CASE("28 test_internal_interrupt_backoff", "[wifi][internal][reconnect]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("InterruptSSID", "pass");
    accessor.test_simulate_disconnect(WIFI_REASON_NO_AP_FOUND);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());

    wm.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::DISCONNECTED, wm.get_state());

    wm.deinit();
}

TEST_CASE("29 test_internal_mixed_stress", "[wifi][internal][stress]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    wm.start();
    wm.connect();
    wm.disconnect();
    wm.stop();
    wm.start();
    wm.connect();

    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(wm.get_state() != WiFiManager::State::UNINITIALIZED);

    wm.deinit();
}

TEST_CASE("30 test_internal_robustness_comprehensive", "[wifi][internal][robustness]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    printf("1. Unexpected events while stopped...\n");
    accessor.test_simulate_ip_event(IP_EVENT_STA_GOT_IP);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::INITIALIZED, wm.get_state());

    printf("2. Unexpected events while started...\n");
    wm.start(5000);
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    wm.deinit();
}

static void concurrent_api_task(void *pvParameters)
{
    WiFiManager &wm = WiFiManager::get_instance();
    for (int i = 0; i < 10; i++) {
        wm.connect();
        vTaskDelay(pdMS_TO_TICKS(5));
        wm.disconnect();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelete(NULL);
}

TEST_CASE("31 test_internal_concurrent_api", "[wifi][internal][concurrency]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    xTaskCreate(concurrent_api_task, "task1", 4096, NULL, 5, NULL);
    xTaskCreate(concurrent_api_task, "task2", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(ESP_OK, wm.deinit());
}

// ========================================================================
// GROUP 6: EXHAUSTIVE FSM MATRIX
// ========================================================================

TEST_CASE("32 test_fsm_matrix_exhaustive", "[wifi][internal][matrix]")
{
    printf("\n=== Test: Exhaustive State Machine Matrix ===\n");
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    struct Step
    {
        WiFiManager::State initial;
        WiFiManager::CommandId cmd;
        esp_err_t expected_ret;
    };

    Step matrix[] = {
        // From INITIALIZED
        {WiFiManager::State::INITIALIZED, WiFiManager::CommandId::CONNECT, ESP_ERR_INVALID_STATE},
        {WiFiManager::State::INITIALIZED, WiFiManager::CommandId::DISCONNECT,
         ESP_ERR_INVALID_STATE},
        {WiFiManager::State::INITIALIZED, WiFiManager::CommandId::STOP,
         ESP_OK}, // STOPPED = INITIALIZED (Idempotent)

        // From STARTED
        {WiFiManager::State::STARTED, WiFiManager::CommandId::START, ESP_OK},      // Idempotent
        {WiFiManager::State::STARTED, WiFiManager::CommandId::DISCONNECT, ESP_OK}, // Already disc

        // From CONNECTING
        {WiFiManager::State::CONNECTING, WiFiManager::CommandId::START, ESP_OK},   // Idempotent
        {WiFiManager::State::CONNECTING, WiFiManager::CommandId::CONNECT, ESP_OK}, // Already conn
    };

    for (auto &step : matrix) {
        printf("Testing State %d -> Command %d\n", (int)step.initial, (int)step.cmd);

        // Preparation to reach initial state
        wm.deinit();
        wm.init();
        if (step.initial != WiFiManager::State::INITIALIZED) {
            wm.start(5000);
            // Redundant: wm.start(5000) already waits for real STA_START event.
            if (step.initial == WiFiManager::State::CONNECTING) {
                wm.set_credentials("SSID", "PASS");
                wm.connect();
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        // Action and Verification
        esp_err_t ret = ESP_FAIL;
        switch (step.cmd) {
        case WiFiManager::CommandId::START:
            ret = wm.start(100);
            break;
        case WiFiManager::CommandId::STOP:
            ret = wm.stop(100);
            break;
        case WiFiManager::CommandId::CONNECT:
            ret = wm.connect(100);
            break;
        case WiFiManager::CommandId::DISCONNECT:
            ret = wm.disconnect(100);
            break;
        default:
            break;
        }
        TEST_ASSERT_EQUAL(step.expected_ret, ret);
    }

    wm.deinit();
}

TEST_CASE("33 test_event_strictness_comprehensive", "[wifi][internal][strict]")
{
    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    printf("1. STA_START while INITIALIZED (must ignore)...\n");
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::INITIALIZED, wm.get_state());

    printf("2. STA_STOP while STARTED (must ignore)...\n");
    wm.start(5000);
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_STOP);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("3. GOT_IP while STARTED (must ignore)...\n");
    accessor.test_simulate_ip_event(IP_EVENT_STA_GOT_IP);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    wm.deinit();
}

TEST_CASE("34 test_rssi_quality_logs", "[wifi][internal][quality]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("QualityTest", "pass");
    accessor.test_simulate_disconnect(WIFI_REASON_BEACON_TIMEOUT, -95); // CRITICAL
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());

    accessor.test_simulate_disconnect(WIFI_REASON_BEACON_TIMEOUT, -80); // MEDIUM
    vTaskDelay(pdMS_TO_TICKS(100));

    accessor.test_simulate_disconnect(WIFI_REASON_BEACON_TIMEOUT, -50); // GOOD
    vTaskDelay(pdMS_TO_TICKS(100));

    wm.deinit();
}

TEST_CASE("35 test_backoff_graceful_shutdown", "[wifi][internal][lifecycle]")
{
    set_memory_leak_threshold(-2000);
    WiFiManager &wm = WiFiManager::get_instance();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("ShutdownSSID", "pass");
    accessor.test_simulate_disconnect(WIFI_REASON_NO_AP_FOUND);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());

    TEST_ASSERT_EQUAL(ESP_OK, wm.deinit());
    TEST_ASSERT_EQUAL(WiFiManager::State::UNINITIALIZED, wm.get_state());
}

#endif // UNIT_TEST
