# WiFiManager API Reference

This document provides a detailed reference for the WiFiManager component API, including the public interface, implementation details, and supported types.

## Public Interface: `IWiFiManager`

The `IWiFiManager` interface defines the high-level API for managing WiFi connectivity on ESP32. It abstracts away the complexity of the WiFi driver, event handling, and credential persistence.

### Lifecycle Management

#### `init(const Config &config = Config{})`
Initializes the WiFi Manager component. Prepares internal resources, initializes NVS, sets up the WiFi driver in Station mode, and starts the background management task.

**Parameters:**
- `config`: [in] Optional `Config` struct specifying `task_stack_size` (default: 4096 bytes) and `task_priority` (default: 5).

**Returns:**
- `ESP_OK`: Success or already initialized.
- `ESP_ERR_NO_MEM`: Failed to allocate memory for internal synchronization objects or task.
- `Other`: Error codes propagated from the underlying NVS or WiFi driver initialization.

#### `deinit`
Deinitializes the WiFi Manager component. Stops any active WiFi connection, terminates the background task, and releases all allocated resources.

**Returns:**
- `ESP_OK`: Success or already deinitialized.
- `Other`: Error codes propagated from the underlying HAL or FreeRTOS operations.

---

### Driver Control

#### `start(uint32_t timeout_ms)`
Synchronously starts the WiFi driver. Blocks until the WiFi driver is successfully started or the timeout occurs.

**Parameters:**
- `timeout_ms`: Maximum time in milliseconds to wait for the operation to complete.

**Returns:**
- `ESP_OK`: WiFi started successfully.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in a state where start is not allowed.
- `ESP_ERR_TIMEOUT`: Operation timed out.
- `ESP_FAIL`: Internal command queue error or driver failure.

#### `start()`
Asynchronously starts the WiFi driver. Queues a start command to the background task and returns immediately.

**Returns:**
- `ESP_OK`: Command successfully queued.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in a state where start is not allowed.
- `ESP_FAIL`: Internal command queue is full.

#### `stop(uint32_t timeout_ms)`
Synchronously stops the WiFi driver. Blocks until the WiFi driver is successfully stopped or the timeout occurs.

**Parameters:**
- `timeout_ms`: Maximum time in milliseconds to wait for the operation to complete.

**Returns:**
- `ESP_OK`: WiFi stopped successfully.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in a state where stop is not allowed.
- `ESP_ERR_TIMEOUT`: Operation timed out.
- `ESP_FAIL`: Internal command queue error or driver failure.

#### `stop()`
Asynchronously stops the WiFi driver. Queues a stop command to the background task and returns immediately.

**Returns:**
- `ESP_OK`: Command successfully queued.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in a state where stop is not allowed.
- `ESP_FAIL`: Internal command queue is full.

---

### Connection Control

#### `connect(uint32_t timeout_ms, uint8_t max_retries = 0, uint32_t base_delay_ms = 1500)`
Synchronously connect to the Access Point using stored credentials with optional automatic retries. Blocks until the connection is established and an IP address is obtained, or all retry attempts are exhausted.

**Parameters:**
- `timeout_ms`: Maximum time in milliseconds to wait for each individual connection attempt.
- `max_retries`: [optional] Number of retry attempts upon failure/timeout (default: 0 = 1 attempt total).
- `base_delay_ms`: [optional] Base delay in milliseconds between retry attempts with linear scaling (default: 1500 ms).

**Returns:**
- `ESP_OK`: Successfully connected and obtained an IP address.
- `ESP_ERR_INVALID_STATE`: Manager not initialized, WiFi not started, or in an invalid state.
- `ESP_ERR_TIMEOUT`: Operation timed out before obtaining an IP address across all attempts.
- `ESP_FAIL`: Connection failed (e.g., rejected by AP or bad credentials).
- `ESP_ERR_WIFI_PASSWORD`: No valid credentials stored in NVS.

#### `connect()`
Asynchronously connect to the Access Point using stored credentials. Queues a connect command to the background task and returns immediately.

**Returns:**
- `ESP_OK`: Command successfully queued.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in an invalid state.
- `ESP_FAIL`: Internal command queue is full.
- `ESP_ERR_WIFI_PASSWORD`: no valid credentials stored.

#### `disconnect(uint32_t timeout_ms)`
Synchronously disconnects from the current Access Point. Blocks until the disconnection is complete or the timeout occurs.

**Parameters:**
- `timeout_ms`: Maximum time in milliseconds to wait for the operation to complete.

**Returns:**
- `ESP_OK`: Successfully disconnected.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or WiFi not connected.
- `ESP_ERR_TIMEOUT`: Operation timed out.
- `ESP_FAIL`: Internal command queue error or driver failure.

#### `disconnect()`
Asynchronously disconnects from the current Access Point. Queues a disconnect command to the background task and returns immediately.

**Returns:**
- `ESP_OK`: Command successfully queued.
- `ESP_ERR_INVALID_STATE`: Manager not initialized or in an invalid state.
- `ESP_FAIL`: Internal command queue is full.

---

### Credential Management

#### `add_credentials(const std::string &ssid, const std::string &password)`
Add or update WiFi credentials in persistent storage. Saves the SSID and password to NVS. If the WiFi driver is currently active, it will be disconnected to apply the new credentials.

**Parameters:**
- `ssid`: The SSID of the Access Point.
- `password`: The password for the Access Point.

**Returns:**
- `ESP_OK`: Credentials successfully saved.
- `ESP_ERR_INVALID_STATE`: Manager not initialized.
- `ESP_ERR_INVALID_ARG`: SSID or password length exceeds the maximum allowed.
- `Other`: Error codes propagated from the underlying NVS operations.

#### `get_credentials(std::string &ssid, std::string &password)`
Retrieve the currently stored WiFi credentials from persistent storage.

**Parameters:**
- `ssid`: [out] String to store the retrieved SSID.
- `password`: [out] String to store the retrieved password.

**Returns:**
- `ESP_OK`: Credentials successfully retrieved.
- `Other`: Error codes propagated from the underlying NVS operations.

#### `clear_credentials()`
Clear all stored WiFi credentials from persistent storage. Erases credentials from NVS and resets the internal retry counters.

**Returns:**
- `ESP_OK`: Credentials successfully cleared.
- `ESP_ERR_INVALID_STATE`: Manager not initialized.
- `Other`: Error codes propagated from the underlying NVS operations.

#### `factory_reset()`
Reset the WiFi Manager and the WiFi driver to factory defaults. Clears all credentials, wipes the NVS namespace, and restores WiFi driver settings.

**Returns:**
- `ESP_OK`: Factory reset successful.
- `ESP_ERR_INVALID_STATE`: Manager not initialized.
- `Other`: Error codes propagated from NVS or WiFi driver operations.

#### `is_credentials_valid()`
Check if valid WiFi credentials are currently stored.

**Returns:**
- `bool`: `true` if valid credentials found, `false` otherwise.

---

### Status & Information

#### `get_state()`
Returns the current internal state of the WiFi Manager.

**Returns:**
- `State`: The current state as a @ref State enum value.

#### `get_task_handle()`
Get the FreeRTOS task handle for the background WiFi task.

**Returns:**
- `TaskHandle_t`: Handle to the internal WiFi task.

#### `get_ap_info(wifi_ap_record_t &info)`
Get information of the connected Access Point.

**Parameters:**
- `info`: [out] Reference to `wifi_ap_record_t` structure to store AP information.

**Returns:**
- `ESP_OK`: Success.
- `ESP_ERR_INVALID_STATE`: Manager not initialized.
- `ESP_ERR_WIFI_NOT_CONNECT`: Station is in disconnect status.
- `Other`: Error codes propagated from the underlying WiFi driver.

#### `get_rssi(int8_t &rssi)`
Get Received Signal Strength Indicator (RSSI) of the connected Access Point in dBm.

**Parameters:**
- `rssi`: [out] Reference to `int8_t` to store the RSSI value (in dBm).

**Returns:**
- `ESP_OK`: Success.
- `ESP_ERR_INVALID_STATE`: Manager not initialized.
- `ESP_ERR_WIFI_NOT_CONNECT`: Station is in disconnect status.
- `Other`: Error codes propagated from the underlying WiFi driver.

---

## Implementation: `WiFiManager`

The `WiFiManager` class is the concrete implementation of `IWiFiManager`. It follows the **Singleton** pattern and uses **Dependency Injection** for its internal sub-components to facilitate unit testing and modularity.

### Singleton Access

```cpp
static WiFiManager &get_instance();
```

---

## Types and Constants

### `Config`
Configuration parameters for initializing the WiFi Manager.

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `task_stack_size` | `uint32_t` | `4096` | Stack size for WiFiManager FreeRTOS task in bytes (recommended: 3072–8192) |
| `task_priority` | `UBaseType_t` | `5` | Priority for WiFiManager FreeRTOS task |

### `State`
Internal states of the WiFi Manager.

| Constant | Description |
| :--- | :--- |
| `UNINITIALIZED` | Initial state before init() |
| `INITIALIZING` | init() in progress |
| `INITIALIZED` / `STOPPED` | Task running, ready for commands |
| `STARTING` | wifi_start() in progress |
| `STARTED` / `DISCONNECTED` | WiFi driver active, not connected |
| `CONNECTING` | wifi_connect() in progress |
| `CONNECTED_NO_IP` | Connected to AP, waiting for IP |
| `CONNECTED_GOT_IP` | Fully connected with valid IP |
| `DISCONNECTING` | wifi_disconnect() in progress |
| `WAITING_RECONNECT` | Waiting for backoff timer to retry |
| `ERROR_CREDENTIALS` | Connection failed due to wrong credentials |
| `STOPPING` | wifi_stop() in progress |

### `CommandId`
Public commands that can be sent to the WiFi Manager.

| Constant | Description |
| :--- | :--- |
| `START` | Start the WiFi driver |
| `STOP` | Stop the WiFi driver |
| `CONNECT` | Connect to the Access Point |
| `DISCONNECT` | Disconnect from the Access Point |

### Synchronization Bits
FreeRTOS Event Group bits used for internal synchronization.

- `STARTED_BIT`: WiFi driver started
- `STOPPED_BIT`: WiFi driver stopped
- `CONNECTED_BIT`: Got IP address
- `DISCONNECTED_BIT`: Disconnected from AP
- `CONNECT_FAILED_BIT`: Connection attempt failed
- `START_FAILED_BIT`: Driver start failed
- `STOP_FAILED_BIT`: Driver stop failed
- `INVALID_STATE_BIT`: Invalid state for requested command
