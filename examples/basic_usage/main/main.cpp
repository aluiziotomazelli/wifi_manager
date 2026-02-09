/**
 * @file main.cpp
 * @brief Basic WiFi Manager Example
 *
 * This example demonstrates the fundamental usage of the WiFiManager component:
 * - Initialize the WiFi manager
 * - Start the WiFi driver
 * - Set network credentials
 * - Connect to the network
 * - Monitor connection status
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.hpp"
#include <stdio.h>

static const char *TAG = "MAIN";

// WiFi credentials - Replace with your network details
// Method 1: Edit directly here (quick start)
// #define WIFI_SSID "YourNetworkSSID"
// #define WIFI_PASS "YourNetworkPassword"

// Method 2: Create a secrets.h file (recommended for development)
// Copy secrets.h.example to secrets.h and edit, then uncomment:
#include "secrets.h"

// Connection timeout in milliseconds
#define CONNECTION_TIMEOUT_MS 15000

extern "C" void app_main(void) {
	// Set log level for components
	// Change to ESP_LOG_DEBUG for more verbose logging
	// Setting maximum log level in menuconfig may be required
	// ESP_LOG_WARN, ESP_LOG_ERROR or ESP_LOG_NONE for less logging
	esp_log_level_set("WiFiManager", ESP_LOG_INFO);

	ESP_LOGI(TAG, "WiFi Manager Basic Example Starting...");

	// Get the singleton instance
	auto &wifi_mgr = WiFiManager::get_instance();

	// Step 1: Initialize the WiFi manager
	ESP_LOGI(TAG, "Initializing WiFi Manager...");
	esp_err_t ret = wifi_mgr.init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize WiFi Manager: %s",
				 esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(TAG, "WiFi Manager initialized successfully");

	// Step 2: Start the WiFi driver (synchronous - waits for completion)
	ESP_LOGI(TAG, "Starting WiFi driver...");
	ret = wifi_mgr.start(5000); // 5 second timeout
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
		wifi_mgr.deinit();
		return;
	}
	ESP_LOGI(TAG, "WiFi driver started successfully");

	// Step 3: Set WiFi credentials
	ESP_LOGI(TAG, "Setting WiFi credentials for SSID: %s", WIFI_SSID);
	ret = wifi_mgr.set_credentials(WIFI_SSID, WIFI_PASS);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set credentials: %s", esp_err_to_name(ret));
		wifi_mgr.stop(5000);
		wifi_mgr.deinit();
		return;
	}
	ESP_LOGI(TAG, "Credentials set successfully");

	// Step 4: Connect to the WiFi network (synchronous - blocks until connected
	// or timeout)
	ESP_LOGI(TAG, "Connecting to WiFi network...");
	ret = wifi_mgr.connect(CONNECTION_TIMEOUT_MS);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Successfully connected to WiFi!");
		ESP_LOGI(
			TAG,
			"You now have an IP address and can communicate over the network");
	} else if (ret == ESP_ERR_TIMEOUT) {
		ESP_LOGW(TAG, "Connection timed out after %d ms",
				 CONNECTION_TIMEOUT_MS);
	} else {
		ESP_LOGE(TAG, "Connection failed: %s", esp_err_to_name(ret));
	}

	// Step 5: Monitor connection status
	while (1) {
		WiFiManager::State state = wifi_mgr.get_state();

		switch (state) {
		case WiFiManager::State::CONNECTED_GOT_IP:
			ESP_LOGI(TAG, "WiFi Status: Connected with IP");
			break;
		case WiFiManager::State::DISCONNECTED:
			ESP_LOGW(TAG, "WiFi Status: Disconnected");
			break;
		case WiFiManager::State::CONNECTING:
			ESP_LOGI(TAG, "WiFi Status: Connecting...");
			break;
		case WiFiManager::State::WAITING_RECONNECT:
			ESP_LOGI(TAG, "WiFi Status: Waiting to reconnect...");
			break;
		case WiFiManager::State::ERROR_CREDENTIALS:
			ESP_LOGE(TAG, "WiFi Status: Invalid credentials!");
			break;
		default:
			ESP_LOGI(TAG, "WiFi Status: Other state");
			break;
		}

		// Check every 10 seconds
		vTaskDelay(pdMS_TO_TICKS(10000));
	}

	// Cleanup (this code won't be reached in this example due to infinite loop)
	// wifi_mgr.disconnect(5000);
	// wifi_mgr.stop(5000);
	// wifi_mgr.deinit();
}