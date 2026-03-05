# WiFi Manager - Basic Usage Example

This example demonstrates the fundamental usage of the WiFiManager component for ESP32.

## What This Example Does

1. **Initializes** the WiFi Manager singleton
2. **Starts** the WiFi driver in station mode
3. **Sets** network credentials (SSID and password)
4. **Connects** to the specified WiFi network
5. **Monitors** the connection status continuously

## Hardware Required

- ESP32 development board
- USB cable for programming and power
- Access to a WiFi network

## How to Use

### Configure the Project

1. Open the `main/main.cpp` file
2. Replace the following defines with your WiFi credentials:
   ```cpp
   #define WIFI_SSID     "YourNetworkSSID"
   #define WIFI_PASSWORD "YourNetworkPassword"
   ```

### Build and Flash

Build the project and flash it to the board:

```bash
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your ESP32's serial port (e.g., `/dev/ttyUSB0` on Linux or `COM3` on Windows).

Press `Ctrl+]` to exit the serial monitor.

## Expected Output

```
I (xxx) wifi_example: WiFi Manager Basic Example Starting...
I (xxx) wifi_example: Initializing WiFi Manager...
I (xxx) wifi_example: WiFi Manager initialized successfully
I (xxx) wifi_example: Starting WiFi driver...
I (xxx) wifi_example: WiFi driver started successfully
I (xxx) wifi_example: Setting WiFi credentials for SSID: YourNetworkSSID
I (xxx) wifi_example: Credentials set successfully
I (xxx) wifi_example: Connecting to WiFi network...
I (xxx) wifi_example: Successfully connected to WiFi!
I (xxx) wifi_example: You now have an IP address and can communicate over the network
I (xxx) wifi_example: WiFi Status: Connected with IP
```

## Troubleshooting

**Connection timeout:**
- Verify your SSID and password are correct
- Check that your WiFi network is operational and in range
- Try increasing `CONNECTION_TIMEOUT_MS` in `main.cpp`

**Failed to initialize:**
- Ensure your ESP-IDF version is 5.x or compatible
- Check that the WiFiManager component is properly included in your project

**Invalid credentials error:**
- Double-check the SSID and password in `main.cpp`
- Ensure there are no extra spaces or special characters

## Key Concepts

### Synchronous vs Asynchronous API

This example uses **synchronous** (blocking) methods:
- `start(timeout_ms)` - Blocks until WiFi driver starts
- `connect(timeout_ms)` - Blocks until connected or timeout

For **asynchronous** (non-blocking) operations, use the parameter-less versions:
- `start()` - Returns immediately
- `connect()` - Returns immediately

Then monitor the state with `get_state()` to check progress.

### State Management

The WiFiManager maintains an internal state machine. Key states:
- `INITIALIZED` - Ready to start
- `STARTED` - WiFi driver running
- `CONNECTING` - Attempting connection
- `CONNECTED_GOT_IP` - Successfully connected with IP
- `DISCONNECTED` - Not connected
- `WAITING_RECONNECT` - Auto-reconnect in progress
- `ERROR_CREDENTIALS` - Invalid SSID/password

### Automatic Reconnection

The WiFiManager automatically attempts to reconnect if the connection is lost, using exponential backoff to avoid overwhelming the network.

## Next Steps

- Explore the **API Reference** (`API.md`) for advanced features
- Check out other examples for asynchronous usage patterns
- Implement your own connection callbacks and event handling