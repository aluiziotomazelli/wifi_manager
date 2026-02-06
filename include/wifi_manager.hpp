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
    enum class State : uint8_t
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
        WAITING_RECONNECT = 9,  ///< Waiting for backoff timer to retry connection.
        ERROR_CREDENTIALS = 10, ///< Last connection failed due to invalid credentials.
        STOPPING          = 11, ///< In the process of stopping the WiFi driver.

        COUNT = 12, ///< Helper for matrix sizing

        // Aliases for readability
        DISCONNECTED = STARTED,
        STOPPED      = INITIALIZED,
    };

    /**
     * @brief Actions returned by the command validator.
     */
    enum class Action
    {
        EXECUTE, ///< Command is valid and should be processed
        SKIP,    ///< Command is idempotent, skip execution
        ERROR,   ///< Command is invalid for the current state
    };

    /**
     * @brief Properties associated with each state.
     */
    struct StateProps
    {
        bool is_active;    ///< WiFi driver is operational (started or connecting/connected)
        bool is_connected; ///< Has an active L2 connection and IP
        bool is_sta_ready; ///< Driver is ready to accept commands (STARTED/CONNECTED)
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

public:
    /**
     * @brief Internal command IDs for the manager task queue.
     */
    enum class CommandId : uint8_t
    {
        START,      ///< Request to start WiFi driver
        STOP,       ///< Request to stop WiFi driver
        CONNECT,    ///< Request to connect to an AP
        DISCONNECT, ///< Request to disconnect from an AP
        EXIT,       ///< Request to terminate the manager task

        COUNT ///< Helper for matrix sizing
    };

    /**
     * @brief Internal event signals mapped from system callbacks.
     */
    enum class EventId : uint8_t
    {
        STA_START,        ///< WiFi station driver started
        STA_STOP,         ///< WiFi station driver stopped
        STA_CONNECTED,    ///< Connected to AP
        STA_DISCONNECTED, ///< Disconnected from AP
        GOT_IP,           ///< Received IP address
        LOST_IP,          ///< IP address lost

        COUNT ///< Helper for transition matrix sizing
    };

    /**
     * @brief Discriminator for the internal message queue.
     */
    enum class MessageType : uint8_t
    {
        COMMAND, ///< Action requested by the user/API
        EVENT,   ///< Signal reported by the system
    };

    /**
     * @brief Structure used to pass commands and events to the internal task.
     */
    struct Message
    {
        MessageType type;
        union
        {
            CommandId cmd;
            EventId event;
        };
        uint8_t reason; ///< Reason code (for STA_DISCONNECTED)
        int8_t rssi;    ///< RSSI level (for STA_DISCONNECTED)
    };

private:
    // Private constructor for singleton
    WiFiManager();
    // Private destructor
    ~WiFiManager();

    // Internal helper to initialize NVS flash partition
    esp_err_t init_nvs();

    // Helper to persist validity flag
    esp_err_t save_valid_flag(bool valid);

    // Main FreeRTOS task loop that executes driver operations
    static void wifi_task(void *pvParameters);

    // Private helper to post messages to the internal queue
    esp_err_t post_message(const Message &msg, bool is_async);

    // Opaque handles for ESP-IDF event handler registrations
    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_t ip_event_instance;

    // Pointer to the default ESP-IDF station network interface
    esp_netif_t *sta_netif;

    // Static callback for WiFi system events (bridged to task)
    static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    // Static callback for IP system events (bridged to task)
    static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    /**
     * @brief Structure defining the result of a state transition from an event.
     */
    struct EventOutcome
    {
        State next_state;        ///< New state to transition to
        EventBits_t bits_to_set; ///< Synchronization bits to release
    };

    /**
     * @brief Validates if a command can be executed in the current state.
     * @param cmd The command to validate.
     * @param current The state to validate against.
     * @return Action Decision for the command (EXECUTE, SKIP, ERROR).
     */
    Action validate_command(CommandId cmd, State current) const;

    /**
     * @brief Resolves the next state and sync bits for a given event.
     * @param event The system event received.
     * @param current The state to validate against.
     * @return EventOutcome The transition logic for the event.
     */
    EventOutcome resolve_event(EventId event, State current) const;

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
    static constexpr EventBits_t ALL_SYNC_BITS = STARTED_BIT | STOPPED_BIT | CONNECTED_BIT | DISCONNECTED_BIT |
                                                 CONNECT_FAILED_BIT | START_FAILED_BIT | STOP_FAILED_BIT |
                                                 INVALID_STATE_BIT;

    // --- State Mask for internal validation ---
    // Properties associated with each state
    static inline constexpr StateProps state_props[(int)State::COUNT] = {
        /* UNINITIALIZED     */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
        /* INITIALIZING      */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
        /* INITIALIZED       */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
        /* STARTING          */ {.is_active = true, .is_connected = false, .is_sta_ready = false},
        /* STARTED           */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
        /* CONNECTING        */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
        /* CONNECTED_NO_IP   */ {.is_active = true, .is_connected = true, .is_sta_ready = true},
        /* CONNECTED_GOT_IP  */ {.is_active = true, .is_connected = true, .is_sta_ready = true},
        /* DISCONNECTING     */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
        /* WAITING_RECONNECT */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
        /* ERROR_CREDENTIALS */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
        /* STOPPING          */ {.is_active = true, .is_connected = false, .is_sta_ready = false},
    };

    // Lookup Table for command validation: [Row: State][Column: CommandId]
    static inline constexpr Action command_matrix[(int)State::COUNT][(int)CommandId::COUNT] = {
        // {START,      STOP,          CONNECT,       DISCONNECT,    EXIT}
        {Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR},      // UNINITIALIZED
        {Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR},      // INITIALIZING
        {Action::EXECUTE, Action::SKIP, Action::ERROR, Action::ERROR, Action::ERROR},     // INITIALIZED
        {Action::SKIP, Action::EXECUTE, Action::ERROR, Action::ERROR, Action::ERROR},     // STARTING
        {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::SKIP, Action::ERROR},    // STARTED
        {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTING
        {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTED_NO_IP
        {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTED_GOT_IP
        {Action::SKIP, Action::EXECUTE, Action::ERROR, Action::SKIP, Action::ERROR},      // DISCONNECTING
        {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::EXECUTE, Action::ERROR}, // WAITING_RECONNECT
        {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::EXECUTE, Action::ERROR}, // ERROR_CREDENTIALS
        {Action::ERROR, Action::SKIP, Action::ERROR, Action::ERROR, Action::ERROR},       // STOPPING
    };

    /**
     * @brief Transition Matrix for Event Handling (GOTO logic).
     * Maps [State][EventId] -> EventOutcome {NextState, SyncBits}
     */
    static inline constexpr EventOutcome transition_matrix[(int)State::COUNT][(int)EventId::COUNT] = {
        // {State, SyncBits}
        /* UNINITIALIZED  */
        {{State::UNINITIALIZED, 0},  // STA_START
         {State::UNINITIALIZED, 0},  // STA_STOP
         {State::UNINITIALIZED, 0},  // STA_CONNECTED
         {State::UNINITIALIZED, 0},  // STA_DISCONNECTED
         {State::UNINITIALIZED, 0},  // GOT_IP
         {State::UNINITIALIZED, 0}}, // LOST_IP
        /* INITIALIZING   */
        {{State::INITIALIZING, 0},  // STA_START
         {State::INITIALIZING, 0},  // STA_STOP
         {State::INITIALIZING, 0},  // STA_CONNECTED
         {State::INITIALIZING, 0},  // STA_DISCONNECTED
         {State::INITIALIZING, 0},  // GOT_IP
         {State::INITIALIZING, 0}}, // LOST_IP
        /* INITIALIZED    */
        {{State::INITIALIZED, 0},  // STA_START
         {State::INITIALIZED, 0},  // STA_STOP
         {State::INITIALIZED, 0},  // STA_CONNECTED
         {State::INITIALIZED, 0},  // STA_DISCONNECTED
         {State::INITIALIZED, 0},  // GOT_IP
         {State::INITIALIZED, 0}}, // LOST_IP
        /* STARTING       */
        {{State::STARTED, STARTED_BIT},          // STA_START
         {State::STARTING, 0},                   // STA_STOP
         {State::STARTING, 0},                   // STA_CONNECTED
         {State::INITIALIZED, START_FAILED_BIT}, // STA_DISCONNECTED
         {State::STARTING, 0},                   // GOT_IP
         {State::STARTING, 0}},                  // LOST_IP
        /* STARTED        */
        {{State::STARTED, 0},  // STA_START
         {State::STARTED, 0},  // STA_STOP
         {State::STARTED, 0},  // STA_CONNECTED (Ignore unexpected)
         {State::STARTED, 0},  // STA_DISCONNECTED
         {State::STARTED, 0},  // GOT_IP
         {State::STARTED, 0}}, // LOST_IP
        /* CONNECTING     */
        {{State::CONNECTING, 0},                   // STA_START
         {State::CONNECTING, 0},                   // STA_STOP
         {State::CONNECTED_NO_IP, 0},              // STA_CONNECTED
         {State::WAITING_RECONNECT, 0},            // STA_DISCONNECTED
         {State::CONNECTED_GOT_IP, CONNECTED_BIT}, // GOT_IP (Early IP acquisition)
         {State::CONNECTING, 0}},                  // LOST_IP
        /* CONNECTED_NO_IP*/
        {{State::CONNECTED_NO_IP, 0},              // STA_START
         {State::CONNECTED_NO_IP, 0},              // STA_STOP
         {State::CONNECTED_NO_IP, 0},              // STA_CONNECTED
         {State::WAITING_RECONNECT, 0},            // STA_DISCONNECTED
         {State::CONNECTED_GOT_IP, CONNECTED_BIT}, // GOT_IP
         {State::CONNECTED_NO_IP, 0}},             // LOST_IP
        /* CONNECTED_GOT_IP*/
        {{State::CONNECTED_GOT_IP, 0},  // STA_START
         {State::CONNECTED_GOT_IP, 0},  // STA_STOP
         {State::CONNECTED_GOT_IP, 0},  // STA_CONNECTED
         {State::WAITING_RECONNECT, 0}, // STA_DISCONNECTED
         {State::CONNECTED_GOT_IP, 0},  // GOT_IP
         {State::CONNECTED_NO_IP, 0}},  // LOST_IP
        /* DISCONNECTING  */
        {{State::DISCONNECTING, 0},          // STA_START
         {State::DISCONNECTING, 0},          // STA_STOP
         {State::DISCONNECTING, 0},          // STA_CONNECTED
         {State::STARTED, DISCONNECTED_BIT}, // STA_DISCONNECTED
         {State::DISCONNECTING, 0},          // GOT_IP
         {State::DISCONNECTING, 0}},         // LOST_IP
        /* WAITING_RECON  */
        {{State::WAITING_RECONNECT, 0},  // STA_START
         {State::WAITING_RECONNECT, 0},  // STA_STOP
         {State::WAITING_RECONNECT, 0},  // STA_CONNECTED
         {State::WAITING_RECONNECT, 0},  // STA_DISCONNECTED
         {State::WAITING_RECONNECT, 0},  // GOT_IP
         {State::WAITING_RECONNECT, 0}}, // LOST_IP
        /* ERROR_CRED     */
        {{State::ERROR_CREDENTIALS, 0},  // STA_START
         {State::ERROR_CREDENTIALS, 0},  // STA_STOP
         {State::ERROR_CREDENTIALS, 0},  // STA_CONNECTED
         {State::ERROR_CREDENTIALS, 0},  // STA_DISCONNECTED
         {State::ERROR_CREDENTIALS, 0},  // GOT_IP
         {State::ERROR_CREDENTIALS, 0}}, // LOST_IP
        /* STOPPING       */
        {{State::STOPPING, 0},              // STA_START
         {State::INITIALIZED, STOPPED_BIT}, // STA_STOP
         {State::STOPPING, 0},              // STA_CONNECTED
         {State::STOPPING, 0},              // STA_DISCONNECTED
         {State::STOPPING, 0},              // GOT_IP
         {State::STOPPING, 0}},             // LOST_IP
    };

    // Coverage verification
    static_assert(sizeof(state_props) / sizeof(state_props[0]) == (int)State::COUNT, "StateProps coverage mismatch");
    static_assert(sizeof(command_matrix) / sizeof(command_matrix[0]) == (int)State::COUNT,
                  "CommandMatrix coverage mismatch");
    static_assert(sizeof(transition_matrix) / sizeof(transition_matrix[0]) == (int)State::COUNT,
                  "TransitionMatrix coverage mismatch");

#ifdef UNIT_TEST
    friend class WiFiManagerTestAccessor;

    // Helpers to create and send specific messages
    esp_err_t test_helper_send_start_command(bool is_async);
    esp_err_t test_helper_send_stop_command(bool is_async);
    esp_err_t test_helper_send_connect_command(bool is_async);
    esp_err_t test_helper_send_disconnect_command(bool is_async);
    esp_err_t test_helper_send_wifi_event(EventId event, uint8_t reason = 0);
    esp_err_t test_helper_send_ip_event(EventId event);

    // Helpers to check queue state
    uint32_t test_helper_get_queue_pending_count() const;
    bool test_helper_is_queue_full() const;
#endif
};
