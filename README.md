# WiFiManager Component

A robust, thread-safe WiFi Station manager for ESP-IDF (v5.x).

## Overview

The `WiFiManager` simplifies WiFi operations on the ESP32 by wrapping the low-level `esp_wifi` driver in a state-machine driven singleton. It handles NVS initialization, event propagation, and provides both synchronous (blocking) and asynchronous (non-blocking) APIs.

## Features

- **Singleton Pattern**: Easy access from anywhere in the application.
- **Dedicated Task**: WiFi operations are decoupled from the main application thread.
- **Automatic Reconnection**: Built-in exponential backoff retry loop for accidental disconnections.
- **Automatic Rollback**: Handles timeouts and connection failures gracefully.
- **Thread-Safe**: Uses Mutexes and Queues for concurrency protection.
- **Persistent Credentials**: Built-in methods for storing/loading credentials in NVS.

## Quick Start

**Credentials:** SSID and PASSWORD can also be set at compile time using Kconfig via `idf.py menuconfig`, under the WIFI SSID Configuration menu.

```cpp
#include "wifi_manager.hpp"

void app_main() {
    auto &wm = WiFiManager::get_instance();

    // 1. Initialize
    wm.init();

    // 2. Start WiFi
    wm.start();

    // 3. Set credentials
    wm.set_credentials("MySSID", "MyPassword");

    // 4. Connect (blocking until IP obtained)
    if (wm.connect(10000) == ESP_OK) {
        printf("Connected successfully!\n");
    }
}
```
 

## Documentation

For a full technical reference of all methods and states, see the [API Reference](API.md).

## Unit Testing

This component includes a comprehensive test suite with over 40 test cases covering:
- **Lifecycle**: Init/Deinit idempotency, Task startup/shutdown.
- **State Machine (FSM)**: Exhaustive command matrix testing in all states, event strictness guards.
- **Credentials**: Set/Get/Clear operations, NVS persistence, and automated validity flag logic.
- **Operational**: Real and simulated connection/disconnection flows (both Sync and Async).
- **Stress & Concurrency**: Queue saturation handling, redundant command spamming, and multi-task API access.
- **Error Handling**: Automated rollbacks on timeouts and credential invalidation on specific failure reasons.

To build the test application:
1. Navigate to the `test` directory of the component.
2. Run `idf.py build`.

Note: Running the tests requires a target ESP32 board.

## License

MIT
