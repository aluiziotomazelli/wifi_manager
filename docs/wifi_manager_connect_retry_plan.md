# Implementation Plan: Synchronous WiFi Connection with Retries in `wifi_manager`

This plan details the enhancement of `wifi_manager` to support synchronous connection retries natively via `WiFiManager::connect(uint32_t timeout_ms, uint8_t max_retries = 0, uint32_t base_delay_ms = 1500)`.

---

## 1. Goal Description

Currently, the synchronous `connect(uint32_t timeout_ms)` performs a single connection attempt. In real-world environments subject to transient RF interference, channel switching, or beacon loss, connection attempts may fail initially but succeed upon an immediate retry with a short backoff delay.

The goal is to enhance `wifi_manager`'s synchronous `connect` API to natively support bounded retries with linear backoff between failed attempts, maintaining 100% backward compatibility with default arguments while keeping the asynchronous `connect()` intact.

---

## 2. Proposed Changes

### [MODIFY] `include/interfaces/i_wifi_manager.hpp`
Update the virtual method signature with default parameters:
```cpp
/**
 * @brief Connect to the configured Access Point synchronously with optional retries.
 *
 * This method sends a CONNECT command to the background task and blocks the caller
 * until the connection succeeds (CONNECTED_GOT_IP), fails, or the timeout expires.
 *
 * If max_retries > 0 and an attempt fails or times out, the method will issue a disconnect,
 * wait with backoff (base_delay_ms * attempt), and retry up to max_retries additional times.
 *
 * @param timeout_ms Timeout for each individual connection attempt in milliseconds.
 * @param max_retries Number of retry attempts upon failure/timeout (default: 0 = 1 attempt total).
 * @param base_delay_ms Base delay in milliseconds between failed attempts (default: 1500 ms).
 *
 * @return
 *     - ESP_OK: Successfully connected and obtained an IP address.
 *     - ESP_ERR_TIMEOUT: Timed out waiting for connection on all attempts.
 *     - ESP_FAIL: Connection failed (e.g., AP rejected connection or bad credentials).
 *     - ESP_ERR_INVALID_STATE: WiFi manager not initialized or not in a connectable state.
 *     - ESP_ERR_WIFI_PASSWORD: No valid credentials stored in NVS.
 */
virtual esp_err_t connect(uint32_t timeout_ms, uint8_t max_retries = 0, uint32_t base_delay_ms = 1500) = 0;
```

---

### [MODIFY] `include/wifi_manager.hpp`
Update `WiFiManager` override declaration:
```cpp
esp_err_t connect(uint32_t timeout_ms, uint8_t max_retries = 0, uint32_t base_delay_ms = 1500) override;
```

---

### [MODIFY] `src/wifi_manager.cpp`
Implement the retry loop with backoff and cleanup inside `connect()`:
```cpp
esp_err_t WiFiManager::connect(uint32_t timeout_ms, uint8_t max_retries, uint32_t base_delay_ms)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    if (!storage_->is_valid())
        return ESP_ERR_WIFI_PASSWORD;
    Action action = state_machine_->validate_command(CommandId::CONNECT);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    const uint8_t total_attempts = max_retries + 1;
    esp_err_t last_err = ESP_FAIL;

    for (uint8_t attempt = 1; attempt <= total_attempts; ++attempt) {
        Message msg = {};
        msg.type = MessageType::COMMAND;
        msg.cmd = CommandId::CONNECT;

        sync_manager_->clear_bits(CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT);
        esp_err_t err = post_message(msg, false);
        if (err != ESP_OK)
            return err;

        uint32_t bits = sync_manager_->wait_for_bits(
            CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT, timeout_ms);

        if (bits & INVALID_STATE_BIT)
            return ESP_ERR_INVALID_STATE;
        if (bits & CONNECTED_BIT)
            return ESP_OK;

        if (bits & CONNECT_FAILED_BIT) {
            last_err = ESP_FAIL;
        } else {
            disconnect();
            last_err = ESP_ERR_TIMEOUT;
        }

        if (attempt < total_attempts) {
            disconnect(2000);
            vTaskDelay(pdMS_TO_TICKS(base_delay_ms * attempt));
        }
    }

    return last_err;
}
```

---

### [MODIFY] `API.md`
Update the `connect(uint32_t timeout_ms, ...)` section:
```markdown
#### `connect(uint32_t timeout_ms, uint8_t max_retries = 0, uint32_t base_delay_ms = 1500)`
Synchronously connect to the Access Point using stored credentials with optional automatic retries. Blocks until the connection is established and an IP address is obtained, or all retry attempts are exhausted.

**Parameters:**
- `timeout_ms`: Maximum time in milliseconds to wait for each individual connection attempt.
- `max_retries`: [optional] Number of retries on failure/timeout (default: 0).
- `base_delay_ms`: [optional] Base delay in milliseconds between retry attempts with linear scaling (default: 1500 ms).

**Returns:**
- `ESP_OK`: Successfully connected and obtained an IP address.
- `ESP_ERR_INVALID_STATE`: Manager not initialized, WiFi not started, or in an invalid state.
- `ESP_ERR_TIMEOUT`: Operation timed out before obtaining an IP address across all attempts.
- `ESP_FAIL`: Connection failed (e.g., rejected by AP or bad credentials).
- `ESP_ERR_WIFI_PASSWORD`: No valid credentials stored in NVS.
```

---

### [MODIFY] `host_test/test_wifi_manager/main/test_wifi_manager_task.cpp`
Add unit test cases in `WiFiManagerTaskTest`:
1. `ConnectSyncSuccessOnFirstAttempt`: Verifies single attempt when `max_retries = 0`.
2. `ConnectSyncFailsThenSucceedsOnSecondAttempt`: Sets `CONNECT_FAILED_BIT` on attempt 1, then `CONNECTED_BIT` on attempt 2 with `max_retries = 1`, verifying `ESP_OK` return.
3. `ConnectSyncExhaustsAllRetriesAndReturnsFail`: Sets `CONNECT_FAILED_BIT` on all attempts, verifying `total_attempts` executions and `ESP_FAIL` return.
4. `ConnectSyncTimeoutRetriesAndReturnsTimeout`: Sets no bits (timeout), verifying `disconnect()` calls and `ESP_ERR_TIMEOUT` return.

---

### [MODIFY] `CHANGELOG.md` & `idf_component.yml`
Document the addition of retry parameters in `connect()` and bump version to `v1.3.0`.

---

## 3. Verification Plan

### Automated Host Tests
Run the complete host test suite:
```bash
cd /home/german/dev/workspaces/idf-components/wifi_manager/host_test
. $HOME/dev/esp/esp-idf/export.sh
cmake -B build -S .
cmake --build build --target build_all_tests
cd build && ctest --output-on-failure
```

### Target Build Verification (Example Application)
Build the `basic_usage` example to verify ESP-IDF firmware compilation:
```bash
cd /home/german/dev/workspaces/idf-components/wifi_manager/examples/basic_usage
. $HOME/dev/esp/esp-idf/export.sh
idf.py build
```
