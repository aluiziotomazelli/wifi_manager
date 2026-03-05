# WiFi Manager Test Applications (Target)

This folder contains ESP-IDF applications designed to test the `wifi_manager` component on real hardware.

## Structure

Each sub-directory is a standalone ESP-IDF project that tests a specific part of the component:
- `integration_internal/`: Tests internal interaction between FSM and the manager task.
- `wifi_config_storage/`: Tests credential persistence in NVS.
- `wifi_driver_hal/`: Tests the Hardware Abstraction Layer with the real Wi-Fi stack.
- `wifi_event_handler/`: Tests the translation of system events to internal messages.
- `wifi_state_machine/`: Tests the logic of the Finite State Machine.
- `wifi_sync_manager/`: Tests thread-safe synchronization and queue management.

## How to Run

To run these tests, you need an ESP32, ESP32-S3, or ESP32-C3 development board.

1. Navigate to the desired test project:
   ```bash
   cd test_apps/<test_folder>
   ```

2. Set the target chip (e.g., esp32):
   ```bash
   idf.py set-target esp32
   ```

3. Build, flash, and monitor:
   ```bash
   idf.py build flash monitor
   ```

The test results will be printed to the serial console using the Unity test framework.
