# Feature Walkthrough: `esp_wifi_sta_get_ap_info` HAL Wrapper and AP Info / RSSI Retrieval

This document summarizes the implementation of `esp_wifi_sta_get_ap_info` wrapper in `IWiFiDriverHAL` / `WiFiDriverHAL`, along with `get_ap_info` and `get_rssi` methods in `IWiFiManager` / `WiFiManager`.

## Overview

Issue [#6](https://github.com/aluiziotomazelli/wifi_manager/issues/6):
Added `wifi_sta_get_ap_info` to `IWiFiDriverHAL` / `WiFiDriverHAL` as a 1:1 wrapper around ESP-IDF's `esp_wifi_sta_get_ap_info`. High-level APIs (`get_ap_info` and `get_rssi`) were added to `IWiFiManager` and `WiFiManager`.

---

## Architectural & Design Decisions

1. **Dual High-Level API (`get_ap_info` and `get_rssi`)**:
   - `esp_err_t get_ap_info(wifi_ap_record_t &info)`: Provides full access to all fields of `wifi_ap_record_t` (SSID, BSSID, RSSI, channel, authmode, etc.).
   - `esp_err_t get_rssi(int8_t &rssi)`: A convenience helper method that delegates to `get_ap_info(info)` and extracts `info.rssi`.

2. **State & Thread-Safety**:
   - `esp_wifi_sta_get_ap_info` is an in-memory, thread-safe query in the ESP-IDF WiFi driver.
   - In `WiFiManager`, calling `get_ap_info` acquires `state_mutex_` to verify that the manager is initialized (`state != State::UNINITIALIZED`) and to guard against concurrent `deinit()`.
   - If the station is not connected, the underlying driver returns `ESP_ERR_WIFI_NOT_CONNECT` directly.

---

## Code Structure

### 1. HAL Layer & Mocks

- **`include/interfaces/i_wifi_driver_hal.hpp`**:
  ```cpp
  /**
   * @copydoc esp_wifi_sta_get_ap_info
   */
  virtual esp_err_t wifi_sta_get_ap_info(wifi_ap_record_t *info) = 0;
  ```

- **`include/wifi_driver_hal.hpp`**:
  ```cpp
  /**
   * @copydoc IWiFiDriverHAL::wifi_sta_get_ap_info()
   */
  esp_err_t wifi_sta_get_ap_info(wifi_ap_record_t *info) override
  {
      return esp_wifi_sta_get_ap_info(info);
  };
  ```

- **`host_test/common/mock_wifi_driver_hal.hpp`**:
  ```cpp
  MOCK_METHOD(esp_err_t, wifi_sta_get_ap_info, (wifi_ap_record_t * info), (override));
  ```

### 2. High-Level Component API

- **`include/interfaces/i_wifi_manager.hpp`**:
  ```cpp
  virtual esp_err_t get_ap_info(wifi_ap_record_t &info) = 0;
  virtual esp_err_t get_rssi(int8_t &rssi) = 0;
  ```

- **`src/wifi_manager.cpp`**:
  ```cpp
  esp_err_t WiFiManager::get_ap_info(wifi_ap_record_t &info)
  {
      xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
      if (state_machine_->get_current_state() == State::UNINITIALIZED) {
          xSemaphoreGiveRecursive(state_mutex_);
          return ESP_ERR_INVALID_STATE;
      }
      esp_err_t err = driver_hal_->wifi_sta_get_ap_info(&info);
      xSemaphoreGiveRecursive(state_mutex_);
      return err;
  }

  esp_err_t WiFiManager::get_rssi(int8_t &rssi)
  {
      wifi_ap_record_t info = {};
      esp_err_t err = get_ap_info(info);
      if (err == ESP_OK) {
          rssi = info.rssi;
      }
      return err;
  }
  ```

---

## Verification & Unit Testing

- Unit test suite added in **`host_test/test_wifi_manager/main/test_wifi_manager_ap_info.cpp`**.
- Tests cover:
  1. `get_ap_info` when uninitialized returns `ESP_ERR_INVALID_STATE`.
  2. `get_ap_info` when disconnected propagates driver error (`ESP_ERR_WIFI_NOT_CONNECT`).
  3. `get_ap_info` when connected populates `wifi_ap_record_t` structure.
  4. `get_rssi` extracts RSSI on success and returns error on driver failure.
- Host tests execution result: **81/81 PASSED**.
