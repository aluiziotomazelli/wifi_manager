# WiFiManager API Reference

The `WiFiManager` is a singleton class designed for robust WiFi management on ESP32 using a dedicated background task.

## Public API

### `static WiFiManager& get_instance()`
Returns the singleton instance of the manager.

### `esp_err_t init()`
Initializes the manager. This must be called before any other operation.
- **Returns**: `ESP_OK` on success, or an error code.
- **Actions**: Sets up NVS, Netif, Event Loop, and starts the internal `wifi_task`.

### `esp_err_t deinit()`
Cleans up all resources.
- **Returns**: `ESP_OK` on success.
- **Actions**: Stops WiFi if running, kills the task, and deletes RTOS objects.

### `esp_err_t start(uint32_t timeout_ms)`
Synchronously starts the WiFi driver in Station mode.
Blocks until the WiFi driver is successfully started or a timeout occurs.
- **Parameters**: `timeout_ms` - Max wait time.
- **Returns**: `ESP_OK`, `ESP_ERR_TIMEOUT`, or `ESP_ERR_INVALID_STATE`.

### `esp_err_t start()`
Asynchronously starts the WiFi driver. Returns immediately after queuing the command.
- **Returns**: `ESP_OK` if the command was queued.

### `esp_err_t stop(uint32_t timeout_ms)`
Synchronously stops the WiFi driver.
- **Returns**: `ESP_OK` or `ESP_ERR_TIMEOUT`.

### `esp_err_t stop()`
Asynchronously stops the WiFi driver.

### `esp_err_t connect(uint32_t timeout_ms)`
Synchronously connects to an Access Point using stored credentials.
Blocks until a connection is established and an IP is obtained.
- **Parameters**:
  - `timeout_ms`: Max time to wait for connection AND IP acquisition.
- **Returns**: `ESP_OK` on success, `ESP_ERR_TIMEOUT` on timeout, or `ESP_FAIL`.

### `esp_err_t connect()`
Asynchronously connects to an Access Point using stored credentials.

### `esp_err_t disconnect(uint32_t timeout_ms)`
Synchronously disconnects from the current network.

### `esp_err_t disconnect()`
Asynchronously disconnects from the current network.

### `State get_state() const`
Returns the current internal state of the manager.

### `esp_err_t set_credentials(const std::string& ssid, const std::string& password)`
Configures WiFi credentials and saves them to the driver's NVS.

### `esp_err_t get_credentials(std::string& ssid, std::string& password)`
Retrieves the currently configured credentials from the driver.

### `esp_err_t clear_credentials()`
Clears credentials from the driver and resets the validity flag.

### `esp_err_t factory_reset()`
Calls `esp_wifi_restore()` and clears all manager settings.

### `bool is_credentials_valid() const`
Returns whether the current credentials are considered valid.

---

## State Enum Reference

| State | Description |
| :--- | :--- |
| `UNINITIALIZED` | Initial state. |
| `INITIALIZED` | Manager task is running, ready for commands. |
| `STARTED` | WiFi driver is active in STA mode. |
| `CONNECTED_GOT_IP` | Fully connected with a valid IP address. |
| `DISCONNECTED` | WiFi is started but not connected to an AP. |
| `WAITING_RECONNECT` | Waiting for backoff timer to retry connection. |
| `ERROR_CREDENTIALS` | Connection failed due to invalid credentials (`AUTH_FAIL`). |
| `...` | Various transitioning states (`STARTING`, `STOPPING`, `CONNECTING`, etc). |

---

## Design Notes

- **Thread Safety**: All public methods use an internal mutex and command queue, making the class safe to use from multiple FreeRTOS tasks.
- **Non-Blocking Task**: The actual work (calling `esp_wifi_*` functions) happens in a dedicated task (`wifi_task`) with priority 5.
- **Rollback Logic**: If a synchronous `start` or `connect` fails or times out, the manager automatically attempts a rollback to a stable state (`STOPPED` or `DISCONNECTED`).
- **Auto-Reconnection**: If the link is lost after a successful connection (Beacon Timeout, AP Reboot, etc.), the manager automatically initiates an exponential backoff retry loop (1s, 2s, 4s... up to 5 min).
- **Credential Protection**: If the driver reports specific failure codes (e.g., `WIFI_REASON_AUTH_FAIL`), the manager transitions to `ERROR_CREDENTIALS` and stops retrying.
