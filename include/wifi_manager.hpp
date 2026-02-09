#pragma once

/**
 * @file wifi_manager.hpp
 * @brief Singleton WiFi Manager for ESP32.
 * @author Jules
 * @version 1.1.0
 */

#include <cstdint>
#include <string>

#include "wifi_config_storage.hpp"
#include "wifi_driver_hal.hpp"
#include "wifi_state_machine.hpp"
#include "wifi_sync_manager.hpp" // Added this include
#include "wifi_types.hpp"

class WiFiManagerTestAccessor;

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
    friend class WiFiManagerTestAccessor;

public:
    using State        = WiFiStateMachine::State;
    using CommandId    = WiFiStateMachine::CommandId;
    using EventId      = WiFiStateMachine::EventId;
    using Action       = WiFiStateMachine::Action;
    using EventOutcome = WiFiStateMachine::EventOutcome;
    using StateProps   = WiFiStateMachine::StateProps;

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

    using MessageType = wifi_manager::MessageType;
    using Message     = wifi_manager::Message;

private:
    // Private constructor for singleton
    WiFiManager();
    // Private destructor
    ~WiFiManager();

    // Internal helper to initialize NVS flash partition
    esp_err_t init_nvs();

    // Helper to persist validity flag (DEPRECATED: used via storage)
    esp_err_t save_valid_flag(bool valid);

    // Main FreeRTOS task loop that executes driver operations
    static void wifi_task(void *pvParameters);

    // Private helper to post messages to the internal queue
    esp_err_t post_message(const Message &msg, bool is_async);

    // --- Sub-components ---
    WiFiConfigStorage storage;
    WiFiStateMachine state_machine;
    WiFiDriverHAL driver_hal;
    wifi_manager::WiFiSyncManager sync_manager;

    // --- Private Members ---
    TaskHandle_t task_handle;              ///< Task handling internal state
    mutable SemaphoreHandle_t state_mutex; ///< Recursive mutex for thread-safe state access

    /**
     * @brief Resolves the next state and sync bits for a given event.
     * @param event The system event received.
     * @return EventOutcome The transition logic for the event.
     */
    EventOutcome resolve_event(EventId event) const;

    // Command Handlers
    void handle_start(const Message &msg, State state);
    void handle_stop(const Message &msg, State state);
    void handle_connect(const Message &msg, State state);
    void handle_disconnect(const Message &msg, State state);

    // Event Handler (LUT-based)
    void handle_event(const Message &msg, State state);

    /**
     * @brief Central dispatcher for all incoming messages.
     * @param msg The message (command or event) to process.
     * @param state The current state of the manager (captured under mutex).
     */
    void process_message(const Message &msg, State state);
};
