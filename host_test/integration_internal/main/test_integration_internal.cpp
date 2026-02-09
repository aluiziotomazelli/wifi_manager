#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_wifi.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// Include the shared accessor
#include "esp_log.h"
#include "test_wifi_manager_accessor.hpp"
#include "wifi_manager.hpp"
#include "host_test_common.hpp"

// Wi-Fi stubs with auto event simulation for integration tests
esp_err_t integration_esp_wifi_start(int cmock_num_calls) {
    if (g_host_test_auto_simulate_events) {
        WiFiManager &wm = WiFiManager::get_instance();
        WiFiManagerTestAccessor accessor(wm);
        accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    }
    return ESP_OK;
}

esp_err_t integration_esp_wifi_stop(int cmock_num_calls) {
    if (g_host_test_auto_simulate_events) {
        WiFiManager &wm = WiFiManager::get_instance();
        WiFiManagerTestAccessor accessor(wm);
        accessor.test_simulate_wifi_event(WIFI_EVENT_STA_STOP);
    }
    return ESP_OK;
}

esp_err_t integration_esp_wifi_connect(int cmock_num_calls) {
    if (g_host_test_auto_simulate_events) {
        WiFiManager &wm = WiFiManager::get_instance();
        WiFiManagerTestAccessor accessor(wm);
        accessor.test_simulate_wifi_event(WIFI_EVENT_STA_CONNECTED);
        accessor.test_simulate_ip_event(IP_EVENT_STA_GOT_IP);
    }
    return ESP_OK;
}

void setUp(void)
{
    host_test_setup_common_mocks();

    // Override default start/stop/connect for integration tests
    esp_wifi_start_Stub(integration_esp_wifi_start);
    esp_wifi_stop_Stub(integration_esp_wifi_stop);
    esp_wifi_connect_Stub(integration_esp_wifi_connect);
}

void tearDown(void)
{
}

TEST_CASE("Internal: Queue Behaviors", "[wifi][internal][stress]")
{
    printf("\n=== Test: Queue Behaviors ===\n");

    nvs_flash_erase();
    nvs_flash_init();

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();

    WiFiManagerTestAccessor accessor(wm);

    const int QUEUE_SIZE = 10;

    // 1. Suspend the consumer task
    accessor.test_suspend_manager_task();

    // 2. Fill the queue
    int successful_sends = 0;
    for (int i = 0; i < QUEUE_SIZE; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, accessor.test_send_start_command(true));
        successful_sends++;
    }
    TEST_ASSERT_EQUAL(QUEUE_SIZE, successful_sends);
    TEST_ASSERT_TRUE(accessor.test_is_queue_full());

    // 3. Verify overflow
    TEST_ASSERT_EQUAL(ESP_FAIL, accessor.test_send_start_command(true));

    // 4. Resume the task
    accessor.test_resume_manager_task();

    // 5. Wait for queue to drain
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_FALSE(accessor.test_is_queue_full());
    TEST_ASSERT_EQUAL(0, accessor.test_get_queue_pending_count());
    wm.deinit();
    nvs_flash_deinit();
}

TEST_CASE("Internal: Connection Flow Simulation", "[wifi][internal][state]")
{
    printf("\n=== Test: Connection Flow Simulation ===\n");
    g_host_test_auto_simulate_events = false; // We want to control events manually here

    nvs_flash_erase();
    nvs_flash_init();

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    printf("Starting WiFi...\n");
    wm.start();
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTING, wm.get_state());

    printf("Simulating WIFI_EVENT_STA_START...\n");
    accessor.test_simulate_wifi_event(WIFI_EVENT_STA_START);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(WiFiManager::State::STARTED, wm.get_state());

    printf("Connecting...\n");
    wm.connect();
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
    nvs_flash_deinit();
}

TEST_CASE("Internal: Auto-Reconnect Simulation", "[wifi][internal][reconnect]")
{
    printf("\n=== Test: Auto-Reconnect Simulation ===\n");

    nvs_flash_erase();
    nvs_flash_init();

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);
    WiFiManagerTestAccessor accessor(wm);

    wm.set_credentials("ReconnectSSID", "pass");

    accessor.test_send_connect_command(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::CONNECTED_GOT_IP, wm.get_state());

    printf("Simulating Beacon Timeout...\n");
    accessor.test_simulate_disconnect(WIFI_REASON_BEACON_TIMEOUT);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(WiFiManager::State::WAITING_RECONNECT, wm.get_state());

    wm.deinit();
    nvs_flash_deinit();
}

TEST_CASE("Internal: Immediate Invalidation", "[wifi][internal][reconnect]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: 3 Strikes", "[wifi][internal][reconnect]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: Interrupt Backoff", "[wifi][internal][reconnect]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: Mixed Stress", "[wifi][internal][stress]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: Robustness Comprehensive", "[wifi][internal][robustness]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
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

TEST_CASE("Internal: Concurrent API", "[wifi][internal][concurrency]")
{
    nvs_flash_erase();
    nvs_flash_init();

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    wm.start(5000);

    xTaskCreate(concurrent_api_task, "task1", 4096, NULL, 5, NULL);
    xTaskCreate(concurrent_api_task, "task2", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(ESP_OK, wm.deinit());
    nvs_flash_deinit();
}

TEST_CASE("Internal: Exhaustive FSM Matrix", "[wifi][internal][matrix]")
{
    printf("\n=== Test: Exhaustive State Machine Matrix ===\n");

    nvs_flash_erase();
    nvs_flash_init();

    WiFiManager &wm = WiFiManager::get_instance();
    wm.deinit();
    wm.init();
    WiFiManagerTestAccessor accessor(wm);

    struct Step
    {
        WiFiManager::State initial;
        wifi_manager::CommandId cmd;
        esp_err_t expected_ret;
    };

    Step matrix[] = {
        // From INITIALIZED
        {WiFiManager::State::INITIALIZED, wifi_manager::CommandId::CONNECT, ESP_ERR_INVALID_STATE},
        {WiFiManager::State::INITIALIZED, wifi_manager::CommandId::DISCONNECT, ESP_ERR_INVALID_STATE},
        {WiFiManager::State::INITIALIZED, wifi_manager::CommandId::STOP, ESP_OK}, // STOPPED = INITIALIZED (Idempotent)

        // From STARTED
        {WiFiManager::State::STARTED, wifi_manager::CommandId::START, ESP_OK},      // Idempotent
        {WiFiManager::State::STARTED, wifi_manager::CommandId::DISCONNECT, ESP_OK}, // Already disc

        // From CONNECTING
        {WiFiManager::State::CONNECTING, wifi_manager::CommandId::START, ESP_OK},   // Idempotent
        {WiFiManager::State::CONNECTING, wifi_manager::CommandId::CONNECT, ESP_OK}, // Already conn
    };

    for (auto &step : matrix) {
        printf("Testing State %d -> Command %d\n", (int)step.initial, (int)step.cmd);

        // Preparation to reach initial state
        wm.deinit();
        wm.init();
        if (step.initial != WiFiManager::State::INITIALIZED) {
            wm.start(5000);
            if (step.initial == WiFiManager::State::CONNECTING) {
                wm.set_credentials("SSID", "PASS");
                wm.connect();
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        // Action and Verification
        esp_err_t ret = ESP_FAIL;
        switch (step.cmd) {
        case wifi_manager::CommandId::START:
            ret = wm.start(100);
            break;
        case wifi_manager::CommandId::STOP:
            ret = wm.stop(100);
            break;
        case wifi_manager::CommandId::CONNECT:
            ret = wm.connect(100);
            break;
        case wifi_manager::CommandId::DISCONNECT:
            ret = wm.disconnect(100);
            break;
        default:
            break;
        }
        TEST_ASSERT_EQUAL(step.expected_ret, ret);
    }

    wm.deinit();
    nvs_flash_deinit();
}

TEST_CASE("Internal: Event Strictness Comprehensive", "[wifi][internal][strict]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: RSSI Quality Logs", "[wifi][internal][quality]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}

TEST_CASE("Internal: Backoff Graceful Shutdown", "[wifi][internal][lifecycle]")
{
    nvs_flash_erase();
    nvs_flash_init();

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
    nvs_flash_deinit();
}
