# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).




## [1.1.0] - 2026-02-10

### Refactor
- **Componentization**: Split `WiFiManager` into specialized single-responsibility classes:
    - `WiFiBootstrapper`: Handles initialization and configuration of the WiFiManager.
    - `WiFiConfigStorage`: Handles NVS persistence and credential management.
    - `WiFiEventHandler`: Translates system events into internal signals.
    - `WiFiMessageProcessor`: Handles message processing, state transitions, commands and their validation.
    - `WiFiStateMachine`: Pure logic component for state transitions and command validation.
    - `WiFiSyncManager`: Centralizes synchronization (queues and event groups).
    - `WiFiManager`: Public API interface for WiFiManager.

- **Google Test**: Added Google Test and Google Mock as a dependency for unit testing. WiFiManager has a test constructor with dependencies injection for easier testing.

### Features
 - New declarative FSM (Finite State Machine) architecture using transition matrices.  
 - **Dynamic Reconnection Strategy**: Implemented RSSI-aware retry limits to distinguish between poor signal and wrong credentials.
 
### Enhancements
 - Improved connection robustness with signal quality (RSSI) awareness.  
 - Implemented exponential backoff for reconnection attempts.  
 - **Enhanced Error Handling**: Handshake and authentication failures are now treated as "suspect", allowing more retries in weak signal conditions before invalidating credentials.
 - **Optimized Connection Speed**: Set driver's internal failure retries to zero, giving the WiFiManager FSM immediate control over reconnection logic.

### Testing
- **Isolated Test Apps**: Created dedicated test applications for each new component (`host_test/test_config_storage`, `host_test/test_state_machine`, etc.).
- **Integrated tests with CTest**: All test can be run at once with `ctest`. Look at [`host_test/README.md`](host_test/README.md) for more details.

## [1.0.0] - 2026-02-02

### Added
- Initial release of WiFi Manager component for ESP32
- Singleton pattern implementation for centralized WiFi management
- Thread-safe WiFi operations using dedicated FreeRTOS task
- Synchronous (blocking) and asynchronous (non-blocking) API methods
- Complete state machine with 14 states for robust connection tracking
- Automatic reconnection with exponential backoff strategy
- WiFi credentials management (set, get, clear)
- NVS-based credential persistence
- Factory reset functionality
- IP address acquisition handling (DHCP/Static)
- Connection validation and error detection
- Support for WiFi station mode (STA)
- Comprehensive state reporting via `get_state()`
- Event-driven architecture using ESP-IDF event system
- Thread-safe state access with mutex protection

### Features
- **State Management**: 14 distinct states including UNINITIALIZED, INITIALIZED, STARTED, CONNECTING, CONNECTED_GOT_IP, DISCONNECTED, WAITING_RECONNECT, ERROR_CREDENTIALS, etc.
- **Flexible API**: Both blocking (with timeout) and non-blocking variants for all major operations
- **Retry Logic**: Built-in reconnection attempts with configurable backoff
- **Credential Validation**: Track and persist credential validity
- **Resource Safety**: Proper initialization and deinitialization of all system resources

[1.0.0]: https://github.com/aluiziotomazelli/wifi_manager/releases/tag/v1.0.0