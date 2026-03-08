# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-07

### Architecture

The core motivation for this release was testability. The original `WiFiManager` was a monolith — init, event handling, command dispatch, NVS, and driver calls all lived in one class with no seams for unit testing. The refactor introduced single-responsibility components connected through interfaces, enabling each piece to be tested in isolation with mocked collaborators.

Two extractions drove most of the improvement:

- **`WiFiBootstrapper`**: Init and deinit were ~80 lines of sequenced `esp_netif`/`esp_event`/`esp_wifi` calls inside `WiFiManager`. Extracting them gave the sequence a clear boundary and made `WiFiManager::init()` three lines. The bootstrapper accepts `TaskFunction_t` as a parameter to avoid a circular dependency back to `WiFiManager`.

- **`WiFiMessageProcessor`**: Command and event handlers were private methods of `WiFiManager`, unreachable by tests except through a live FreeRTOS task and queue. Extracting them as a public class with an interface made every handler directly testable, achieving 100% line coverage without FreeRTOS involvement.

The `WiFiDriverHAL` was simplified to pure 1:1 delegation — no state, no policy. All sequencing logic moved to `WiFiBootstrapper`. This made the HAL trivially mockable and removed the need to test it directly.

### Added
- `add_credentials(ssid, password)`: stores up to 10 SSID/password pairs in NVS; adding a duplicate SSID updates its password in place; if the list is full, the last slot is silently overwritten
- `set_credentials` retained as a compatibility alias for `add_credentials`
- NVS initialized internally by `WiFiConfigStorage::init()` — including erase/reinit on invalid or version-mismatched partitions; no `nvs_flash_init()` required in application code
- RSSI-aware credential invalidation: auth failures trigger `handle_suspect_failure(rssi)` which applies signal-dependent retry limits (1 retry at ≥ -55 dBm, 2 at ≥ -67, 5 at ≥ -80) before invalidating credentials — prevents false invalidation under weak signal
- Exponential backoff for reconnection attempts
- `ERROR_CREDENTIALS` state: safe idle after confirmed credential failure; no reconnection attempted; reserved entry point for future AP scanning and provisioning
- Google Test / Google Mock as host test dependency
- Dedicated test application per component (`host_test/test_wifi_manager`, `host_test/test_state_machine`, etc.)
- Full test suite runnable via `ctest` — see [`host_test/README.md`](host_test/README.md)
- Coverage report published to GitHub Pages on every push

### Refactored
- `WiFiManager` decomposed into six single-responsibility collaborators injected via interfaces:
  - `WiFiBootstrapper` — init/deinit sequence and task creation
  - `WiFiConfigStorage` — NVS persistence and credential management
  - `WiFiEventHandler` — ESP-IDF event translation to internal messages
  - `WiFiMessageProcessor` — command and event dispatch, state transition side effects
  - `WiFiStateMachine` — pure FSM logic, transition matrices, backoff calculation
  - `WiFiSyncManager` — FreeRTOS queue and event group encapsulation
- FSM rewritten as declarative transition matrices (State × Event → NextState, State × Command → Action)
- Driver's internal failure retry count set to zero — FSM has full control over reconnection timing
- Kconfig removed — credentials set exclusively via `add_credentials()`

---

## [1.0.0] - 2026-02-02

### Added
- Initial release of WiFiManager component for ESP32
- Singleton pattern for centralized WiFi management
- Thread-safe WiFi operations via dedicated FreeRTOS task
- Synchronous (blocking) and asynchronous (non-blocking) API for all major operations
- State machine with 14 states for connection tracking
- Automatic reconnection with exponential backoff
- Credential management: set, get, clear, factory reset
- NVS-based credential persistence with validity flag
- IP acquisition handling (DHCP)
- Event-driven architecture via ESP-IDF event system
- Mutex-protected state access

[1.0.0]: https://github.com/aluiziotomazelli/wifi_manager/releases/tag/v1.0.0