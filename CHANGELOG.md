# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).




## [1.1.0] - 2026-02-06

### Features
 - New declarative FSM (Finite State Machine) architecture using transition matrices.  
 
### Enhancements
 - Improved connection robustness with signal quality (RSSI) awareness.  
 - Implemented exponential backoff for reconnection attempts.  

### Refactor
 - Unified command and event handling into a serialized message queue.  


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