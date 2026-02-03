#pragma once

/**
 * @file wifi_manager.hpp
 * @brief Singleton WiFi Manager for ESP32.
 * @author Jules
 * @version 1.1.0
 */

#include <cstdint>
#include <string>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Forward declaration for tests
#ifdef UNIT_TEST
class WiFiManagerTestAccessor;
#endif

/**
 * @class WiFiManager
 * @brief Singleton class for managing WiFi connections on ESP32.
 *
 * This class uses a dedicated FreeRTOS task to handle all WiFi operations,
 * ensuring thread safety and a non-blocking internal architecture.
 * It provides both synchronous (blocking) and asynchronous (non-blocking) methods.
 */
class WiFiManager
{
#ifdef UNIT_TEST
    friend class WiFiManagerTestAccessor;
#endif

public:
    /**
     * @brief Internal states of the WiFi manager.
     */
    enum class State
    {
        UNINITIALIZED     = 0,  ///< Initial state before init() is called.
        INITIALIZING      = 1,  ///< In the process of setting up resources.
        INITIALIZED       = 2,  ///< Resources allocated, task running, driver not started.
        STARTING          = 3,  ///< In the process of starting the WiFi driver.
        STARTED           = 4,  ///< WiFi driver started in STA mode.
        CONNECTING        = 5,  ///< Attempting to connect to an AP.
        CONNECTED_NO_IP   = 6,  ///< Connected to AP, waiting for DHCP/Static IP.
        CONNECTED_GOT_IP  = 7,  ///< Successfully connected and has an IP address.
        DISCONNECTING     = 8,  ///< In the process of disconnecting from the AP.
        DISCONNECTED      = 9,  ///< Not connected to any AP.
        WAITING_RECONNECT = 10, ///< Waiting for backoff timer to retry connection.
        ERROR_CREDENTIALS = 11, ///< Last connection failed due to invalid credentials.
        STOPPING          = 12, ///< In the process of stopping the WiFi driver.
        STOPPED           = 13, ///< WiFi driver stopped.
    };

    /**
     * @brief Get the singleton instance of WiFiManager.
     * @return Reference to the WiFiManager instance.
     */
    static WiFiManager &get_instance();

    // Prevent copying and assignment
    WiFiManager(const WiFiManager &)            = delete;
    WiFiManager &operator=(const WiFiManager &) = delete;

    /**
     * @brief Initialize the WiFi stack.
     *
     * Initializes NVS, Netif, Event Loop, creates the command queue,
     * event group, and launches the internal manager task.
     *
     * @return
     *  - ESP_OK: Success.
     *  - ESP_ERR_NO_MEM: Failed to allocate RTOS resources.
     *  - Others: Failed to initialize system-level components.
     */
    esp_err_t init();

    /**
     * @brief Deinitialize the WiFi stack.
     *
     * Stops the WiFi driver if running, terminates the manager task,
     * and releases all allocated RTOS and system resources.
     *
     * @return ESP_OK on success.
     */
    esp_err_t deinit();

    /**
     * @brief Start the WiFi station mode (synchronous).
     *
     * Blocks until the WiFi driver is successfully started or a timeout occurs.
     * Note: This operation can take a few hundred milliseconds.
     *
     * @param timeout_ms Maximum time to wait for the operation to complete.
     * @return
     *  - ESP_OK: Started successfully.
     *  - ESP_ERR_TIMEOUT: Operation timed out.
     *  - ESP_ERR_INVALID_STATE: Manager is not initialized.
     */
    esp_err_t start(uint32_t timeout_ms);

    /**
     * @brief Start the WiFi station mode (asynchronous).
     *
     * Returns immediately after queuing the start command.
     *
     * @return ESP_OK if the command was sent successfully.
     */
    esp_err_t start();

    /**
     * @brief Stop the WiFi station mode (synchronous).
     *
     * Blocks until the WiFi driver is stopped or a timeout occurs.
     *
     * @param timeout_ms Maximum time to wait for the operation to complete.
     * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout.
     */
    esp_err_t stop(uint32_t timeout_ms);

    /**
     * @brief Stop the WiFi station mode (asynchronous).
     *
     * Returns immediately after queuing the stop command.
     *
     * @return ESP_OK if the command was sent successfully.
     */
    esp_err_t stop();

    /**
     * @brief Connect to the configured WiFi network (synchronous).
     *
     * Blocks until a connection is established and an IP is obtained,
     * or a timeout/error occurs. Uses the credentials already stored in the WiFi
     * driver.
     * Note: This can take several seconds depending on network conditions.
     *
     * @param timeout_ms Maximum time to wait for connection and IP.
     * @return
     *  - ESP_OK: Connected and has IP.
     *  - ESP_ERR_TIMEOUT: Failed to connect within the time limit.
     *  - ESP_FAIL: Driver reported immediate failure or no credentials found.
     */
    esp_err_t connect(uint32_t timeout_ms);

    /**
     * @brief Connect to the configured WiFi network (asynchronous).
     *
     * Returns immediately after queuing the connect command.
     *
     * @return ESP_OK if the command was sent successfully.
     */
    esp_err_t connect();

    /**
     * @brief Disconnect from the current WiFi network (synchronous).
     *
     * Blocks until the disconnection is confirmed by the driver.
     *
     * @param timeout_ms Maximum time to wait for the operation to complete.
     * @return ESP_OK on success.
     */
    esp_err_t disconnect(uint32_t timeout_ms);

    /**
     * @brief Disconnect from the current WiFi network (asynchronous).
     *
     * Returns immediately after queuing the disconnect command.
     *
     * @return ESP_OK if the command was sent successfully.
     */
    esp_err_t disconnect();

    /**
     * @brief Get the current state of the WiFi manager.
     * @return The current State enum value.
     */
    State get_state() const;

    /**
     * @brief Set WiFi credentials and save them to the driver's NVS.
     *
     * @param ssid The network SSID.
     * @param password The network password.
     * @return ESP_OK on success.
     */
    esp_err_t set_credentials(const std::string &ssid, const std::string &password);

    /**
     * @brief Get the currently configured WiFi credentials from the driver.
     *
     * @param ssid Output parameter for the SSID.
     * @param password Output parameter for the password.
     * @return ESP_OK on success.
     */
    esp_err_t get_credentials(std::string &ssid, std::string &password);

    /**
     * @brief Clear WiFi credentials from the driver and reset validity flag.
     * @return ESP_OK on success.
     */
    esp_err_t clear_credentials();

    /**
     * @brief Perform a factory reset of the WiFi settings.
     *
     * Calls esp_wifi_restore() and clears the internal "wifi_manager" NVS.
     * @return ESP_OK on success.
     */
    esp_err_t factory_reset();

    /**
     * @brief Check if the currently stored credentials are considered valid.
     * @return true if valid.
     */
    bool is_credentials_valid() const;

private:
    // Private constructor for singleton
    WiFiManager();
    // Private destructor
    ~WiFiManager();

    // Internal helper to initialize NVS flash partition
    esp_err_t init_nvs();

    // --- Internal Machinery ---

    // Internal command IDs for the manager task queue
    enum class CommandId
    {
        START,             // Request to start WiFi driver
        STOP,              // Request to stop WiFi driver
        CONNECT,           // Request to connect to an AP
        DISCONNECT,        // Request to disconnect from an AP
        HANDLE_EVENT_WIFI, // Bridge for WiFi system events
        HANDLE_EVENT_IP,   // Bridge for IP system events
        EXIT,              // Request to terminate the manager task
    };

    // Structure used to pass commands and data to the internal task
    struct Command
    {
        CommandId id;     // The operation requested
        int32_t event_id; // Event ID for HANDLE_EVENT_* commands
        uint8_t reason;   // Reason code for DISCONNECTED events
    };

private:
    // FreeRTOS Event Group bits for synchronization between the API and the task
    static constexpr EventBits_t STARTED_BIT        = BIT0; ///< WiFi driver started
    static constexpr EventBits_t STOPPED_BIT        = BIT1; ///< WiFi driver stopped
    static constexpr EventBits_t CONNECTED_BIT      = BIT2; ///< Got IP address
    static constexpr EventBits_t DISCONNECTED_BIT   = BIT3; ///< Disconnected from AP
    static constexpr EventBits_t CONNECT_FAILED_BIT = BIT4; ///< Connection attempt failed
    static constexpr EventBits_t START_FAILED_BIT   = BIT5; ///< Driver start failed
    static constexpr EventBits_t STOP_FAILED_BIT    = BIT6; ///< Driver stop failed
    static constexpr EventBits_t INVALID_STATE_BIT  = BIT7; ///< Invalid state

    // Mask for all synchronization bits
    static constexpr EventBits_t ALL_SYNC_BITS =
        STARTED_BIT | STOPPED_BIT | CONNECTED_BIT | DISCONNECTED_BIT | CONNECT_FAILED_BIT |
        START_FAILED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT;

    // Main FreeRTOS task loop that executes driver operations
    static void wifi_task(void *pvParameters);

    // Opaque handles for ESP-IDF event handler registrations
    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_t ip_event_instance;

    // Pointer to the default ESP-IDF station network interface
    esp_netif_t *sta_netif;

    // Static callback for WiFi system events (bridged to task)
    static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    // Static callback for IP system events (bridged to task)
    static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    // Private helper to post commands to the internal queue
    esp_err_t send_command(const Command &cmd, bool is_async);

    // FreeRTOS Task handle for the manager loop
    TaskHandle_t task_handle;

    // FreeRTOS Queue handle for command passing
    QueueHandle_t command_queue;

    // FreeRTOS Event Group handle for API synchronization
    EventGroupHandle_t wifi_event_group;

    // Mutex to protect 'current_state' access across tasks
    mutable SemaphoreHandle_t state_mutex;

    // The current thread-protected internal state
    State current_state;

    // Validity of the current credentials (persisted in NVS)
    bool is_credential_valid;

    // Reconnection tracking
    uint32_t retry_count;
    uint32_t suspect_retry_count;
    uint64_t next_reconnect_ms;

    // Helper to persist validity flag
    esp_err_t save_valid_flag(bool valid);

#ifdef UNIT_TEST
    friend class WiFiManagerTestAccessor;

    // Helpers to create and send specific commands
    esp_err_t test_helper_send_start_command(bool is_async);
    esp_err_t test_helper_send_stop_command(bool is_async);
    esp_err_t test_helper_send_connect_command(bool is_async);
    esp_err_t test_helper_send_disconnect_command(bool is_async);

    // Helpers to check queue state
    uint32_t test_helper_get_queue_pending_count() const;
    bool test_helper_is_queue_full() const;
#endif
};
