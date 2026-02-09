# WiFi Manager Host Tests

This folder contains unit and integration tests configured to run directly on a Linux Host, using hardware mocks and the real NVS implementation for persistence.

## Structure

- `common/`: Contains shared utilities, global stubs, and manual mocks for ESP-IDF APIs.
- `wifi_*/`: Individual test projects for each sub-component.
- `integration_internal/`: Integration tests for internal FSM logic and queue management.
- `pytest_host_tests.py`: Automation script for building and running the entire suite.

## How to Run

Ensure the ESP-IDF environment is loaded (`. export.sh`).

### Run all tests
```bash
python -m pytest pytest_host_tests.py
```

### Run a specific test
```bash
cd <test_folder>
idf.py --preview set-target linux
idf.py build
./build/*.elf
```

## Benefits of this Architecture
1. **Isolation**: Each suite runs in its own project, avoiding mock conflicts.
2. **Speed**: Execution in milliseconds without the need to flash hardware.
3. **Real Persistence**: The use of host `nvs_flash` allows validating if credentials actually survive simulated reboots.
4. **CI-Ready**: The `pytest` script generates reports compatible with CI tools like GitHub Actions.
