#include <cstring>

#include "esp_event.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_event_handler.hpp"
#include "wifi_manager.hpp"

static const char *TAG = "WiFiManager";

// =================================================================================================
// Singleton and Constructor/Destructor
// =================================================================================================

WiFiManager &WiFiManager::get_instance()
{
    static WiFiManager instance;
    return instance;
}

WiFiManager::WiFiManager()
    : storage(driver_hal, "wifi_manager")
    , state_machine()
    , driver_hal()
{
    // Mutex is created once and persists for the lifetime of the singleton.
    state_mutex = xSemaphoreCreateRecursiveMutex();
}

WiFiManager::~WiFiManager()
{
    // Cleanup if the object is ever destroyed (singleton usually lasts until shutdown)
    if (task_handle != nullptr) {
        vTaskDelete(task_handle);
    }
    sync_manager.deinit();
    if (state_mutex != nullptr) {
        vSemaphoreDelete(state_mutex);
    }
}

// =================================================================================================
// Public API
// =================================================================================================

esp_err_t WiFiManager::init_nvs()
{
    // NVS is required for the WiFi driver to store internal configurations/calibration
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t WiFiManager::init()
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (state_machine.get_current_state() != State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        ESP_LOGI(TAG, "Already initialized or initializing.");
        return ESP_OK;
    }
    // Set to INITIALIZING to prevent concurrent init calls
    state_machine.transition_to(State::INITIALIZING);
    xSemaphoreGiveRecursive(state_mutex);

    // Global NVS init - and component storage init
    esp_err_t err = storage.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Storage/NVS: %s", esp_err_to_name(err));
        return err;
    }

    // 4. Global Netif init via HAL
    err = driver_hal.init_netif();
    if (err != ESP_OK)
        return err;

    // 5. Default event loop via HAL
    err = driver_hal.create_default_event_loop();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 6. Setup Station Netif via HAL
    err = driver_hal.setup_sta_netif();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 7. Initialize WiFi via HAL
    err = driver_hal.init_wifi();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 8. Set mode to STA via HAL
    err = driver_hal.set_mode_sta();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 9. Initialize synchronization primitives
    err = sync_manager.init();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 10. Register event handlers via HAL
    err =
        driver_hal.register_event_handlers(&wifi_manager::WiFiEventHandler::wifi_event_handler,
                                           &wifi_manager::WiFiEventHandler::ip_event_handler, sync_manager.get_queue());
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // 11. Ensure driver is configured, fallback to Kconfig if necessary
    storage.ensure_config_fallback();

    // 12. Launch the consumer task that executes all driver operations
    BaseType_t task_created = xTaskCreate(wifi_task, "wifi_task", 4096, this, 5, &task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi task");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    state_machine.transition_to(State::INITIALIZED);
    xSemaphoreGiveRecursive(state_mutex);
    ESP_LOGI(TAG, "WiFi Manager initialized.");
    return ESP_OK;
}

esp_err_t WiFiManager::deinit()
{
    State state = get_state();
    ESP_LOGI(TAG, "Deinitializing WiFi Manager...");
    if (state == State::UNINITIALIZED) {
        ESP_LOGI(TAG, "Already uninitialized.");
        return ESP_OK;
    }

    // 1. Ensure WiFi is stopped before deinitializing the stack
    if (state_machine.is_active()) {
        ESP_LOGI(TAG, "WiFi is running, stopping first...");
        stop(2000);
    }

    // 2. Terminate the manager task gracefully using the EXIT command
    if (task_handle != nullptr) {
        ESP_LOGI(TAG, "Stopping WiFi task...");
        Message msg = {};
        msg.type    = MessageType::COMMAND;
        msg.cmd     = CommandId::EXIT;
        if (sync_manager.is_initialized() && sync_manager.post_message(msg) == ESP_OK) {
            // Wait for the task to self-delete and nullify its handle
            int retry = 0;
            while (task_handle != nullptr && retry < 100) {
                vTaskDelay(pdMS_TO_TICKS(10));
                retry++;
            }
            // Small safety delay to ensure FreeRTOS has finished task cleanup
            if (task_handle == nullptr) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        // Forced deletion if graceful exit fails
        if (task_handle != nullptr) {
            ESP_LOGW(TAG, "WiFi task did not exit gracefully, deleting...");
            vTaskDelete(task_handle);
            task_handle = nullptr;
        }
        ESP_LOGI(TAG, "WiFi task terminated.");
    }

    // 3. Deinit the driver stack via HAL
    esp_err_t ret = driver_hal.deinit();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi stack deinitialized.");
    }

    // 4. Unregister event handlers via HAL
    driver_hal.unregister_event_handlers();

    // 5. Clean up internal RTOS synchronization objects
    sync_manager.deinit();

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    state_machine.transition_to(State::UNINITIALIZED);
    xSemaphoreGiveRecursive(state_mutex);

    ESP_LOGI(TAG, "WiFi Manager deinitialized.");
    return ESP_OK;
}

esp_err_t WiFiManager::start(uint32_t timeout_ms)
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::START);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to start WiFi (sync)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::START;

    sync_manager.clear_bits(wifi_manager::STARTED_BIT | wifi_manager::START_FAILED_BIT |
                            wifi_manager::INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK) {
        return err;
    }

    // Wait for the Task to set the success or failure bit
    uint32_t bits = sync_manager.wait_for_bits(
        wifi_manager::STARTED_BIT | wifi_manager::START_FAILED_BIT | wifi_manager::INVALID_STATE_BIT, timeout_ms);

    if (bits & wifi_manager::INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & wifi_manager::STARTED_BIT) {
        return ESP_OK;
    }
    if (bits & wifi_manager::START_FAILED_BIT) {
        return ESP_FAIL;
    }

    // Rollback: if we timed out waiting for the driver, try to stop it to reset state
    ESP_LOGW(TAG, "Start timed out, cancelling...");
    stop();
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::start()
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::START);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to start WiFi (async)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::START;
    return post_message(msg, true);
}

esp_err_t WiFiManager::stop(uint32_t timeout_ms)
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::STOP);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "API: Requesting to stop WiFi (sync)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::STOP;

    sync_manager.clear_bits(wifi_manager::STOPPED_BIT | wifi_manager::STOP_FAILED_BIT |
                            wifi_manager::INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t bits = sync_manager.wait_for_bits(
        wifi_manager::STOPPED_BIT | wifi_manager::STOP_FAILED_BIT | wifi_manager::INVALID_STATE_BIT, timeout_ms);

    if (bits & wifi_manager::INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & wifi_manager::STOPPED_BIT) {
        return ESP_OK;
    }
    if (bits & wifi_manager::STOP_FAILED_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::stop()
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::STOP);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to stop WiFi (async)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::STOP;
    return post_message(msg, true);
}

esp_err_t WiFiManager::connect(uint32_t timeout_ms)
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::CONNECT);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to connect (sync)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::CONNECT;

    sync_manager.clear_bits(wifi_manager::CONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT |
                            wifi_manager::INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK) {
        return err;
    }

    // Wait for either the GOT_IP event (SUCCESS) or a DISCONNECT/ERROR event (FAIL)
    uint32_t bits = sync_manager.wait_for_bits(
        wifi_manager::CONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT | wifi_manager::INVALID_STATE_BIT, timeout_ms);

    if (bits & wifi_manager::INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & wifi_manager::CONNECTED_BIT) {
        return ESP_OK;
    }
    else if (bits & wifi_manager::CONNECT_FAILED_BIT) {
        return ESP_FAIL;
    }
    else {
        // Rollback: if timeout occurs, cancel the driver connection attempt
        ESP_LOGW(TAG, "Connect timed out, cancelling attempt...");
        disconnect();
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t WiFiManager::connect()
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::CONNECT);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to connect (async)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::CONNECT;
    return post_message(msg, true);
}

esp_err_t WiFiManager::disconnect(uint32_t timeout_ms)
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::DISCONNECT);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to disconnect (sync)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::DISCONNECT;

    sync_manager.clear_bits(wifi_manager::DISCONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT |
                            wifi_manager::INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t bits = sync_manager.wait_for_bits(wifi_manager::DISCONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT |
                                                   wifi_manager::INVALID_STATE_BIT,
                                               timeout_ms);

    if (bits & wifi_manager::INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & wifi_manager::DISCONNECTED_BIT) {
        return ESP_OK;
    }
    if (bits & wifi_manager::CONNECT_FAILED_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::disconnect()
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    Action action = state_machine.validate_command(CommandId::DISCONNECT);

    if (action == Action::ERROR) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action == Action::SKIP) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "API: Requesting to disconnect (async)...");
    Message msg = {};
    msg.type    = MessageType::COMMAND;
    msg.cmd     = CommandId::DISCONNECT;
    return post_message(msg, true);
}

WiFiManager::State WiFiManager::get_state() const
{
    // The Mutex ensures that we don't read the state while the Task is mid-transition
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    State state = state_machine.get_current_state();
    xSemaphoreGiveRecursive(state_mutex);
    return state;
}

// =================================================================================================
// Credentials and Reset
// =================================================================================================

esp_err_t WiFiManager::set_credentials(const std::string &ssid, const std::string &password)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (state_machine.get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "API: Setting credentials...");

    // If we are currently active, we must stop the current connection first
    if (state_machine.is_active()) {
        ESP_LOGI(TAG, "Disconnecting before applying new credentials...");
        driver_hal.disconnect();
    }

    esp_err_t err = storage.save_credentials(ssid, password);
    if (err == ESP_OK) {
        state_machine.reset_retries();

        // Apply credentials to the driver via HAL
        wifi_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        strncpy((char *)cfg.sta.ssid, ssid.c_str(), sizeof(cfg.sta.ssid));
        strncpy((char *)cfg.sta.password, password.c_str(), sizeof(cfg.sta.password));

        driver_hal.set_config(&cfg);
        ESP_LOGI(TAG, "Credentials applied successfully.");
    }
    else {
        ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(err));
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t WiFiManager::get_credentials(std::string &ssid, std::string &password)
{
    return storage.load_credentials(ssid, password);
}

esp_err_t WiFiManager::clear_credentials()
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (state_machine.get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "API: Clearing credentials...");

    esp_err_t err = storage.clear_credentials();
    if (err == ESP_OK) {
        state_machine.reset_retries();
    }
    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t WiFiManager::factory_reset()
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (state_machine.get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "API: Factory reset...");
    esp_err_t err = storage.factory_reset();

    state_machine.reset_retries();
    state_machine.transition_to(State::INITIALIZED);

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

bool WiFiManager::is_credentials_valid() const
{
    return storage.is_valid();
}

WiFiManager::EventOutcome WiFiManager::resolve_event(EventId event) const
{
    return state_machine.resolve_event(event);
}

esp_err_t WiFiManager::save_valid_flag(bool valid)
{
    return storage.save_valid_flag(valid);
}

// =================================================================================================
// Internal Implementation
// =================================================================================================

esp_err_t WiFiManager::post_message(const Message &msg, bool is_async)
{
    if (!sync_manager.is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    // For async calls, we use the non-blocking post_message
    // For sync calls, we could enhance WiFiSyncManager to support blocking, but for now we use the same method
    esp_err_t err = sync_manager.post_message(msg);
    if (err != ESP_OK && msg.type == MessageType::COMMAND) {
        ESP_LOGE(TAG, "Failed to send command to queue (full?)");
    }
    return err;
}

void WiFiManager::process_message(const Message &msg, State state)
{
    if (msg.type == MessageType::COMMAND) {
        // Any explicit user command resets the retry counters (except EXIT)
        if (msg.cmd != CommandId::EXIT) {
            state_machine.reset_retries();
        }

        switch (msg.cmd) {
        case CommandId::START:
            handle_start(msg, state);
            break;
        case CommandId::STOP:
            handle_stop(msg, state);
            break;
        case CommandId::CONNECT:
            handle_connect(msg, state);
            break;
        case CommandId::DISCONNECT:
            handle_disconnect(msg, state);
            break;
        default:
            break;
        }
    }
    else {
        // Handle system event via Transition Matrix
        handle_event(msg, state);
    }
}

void WiFiManager::handle_start(const Message &msg, State state)
{
    state_machine.transition_to(State::STARTING);
    esp_err_t err = driver_hal.start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(err));
        state_machine.transition_to(state);
        sync_manager.set_bits(wifi_manager::START_FAILED_BIT);
    }
}

void WiFiManager::handle_stop(const Message &msg, State state)
{
    state_machine.transition_to(State::STOPPING);
    esp_err_t err = driver_hal.stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop wifi: %s", esp_err_to_name(err));
        state_machine.transition_to(state);
        sync_manager.set_bits(wifi_manager::STOP_FAILED_BIT);
    }
}

void WiFiManager::handle_connect(const Message &msg, State state)
{
    state_machine.transition_to(State::CONNECTING);
    esp_err_t err = driver_hal.connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect wifi: %s", esp_err_to_name(err));
        state_machine.transition_to(state);
        sync_manager.set_bits(wifi_manager::CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_disconnect(const Message &msg, State state)
{
    // SPECIAL CASE: Rollback during early connect phase or backoff.
    if (state == State::WAITING_RECONNECT || state == State::CONNECTING) {
        state_machine.transition_to(State::DISCONNECTED);
        driver_hal.disconnect();
        sync_manager.set_bits(wifi_manager::DISCONNECTED_BIT);
        return;
    }

    state_machine.transition_to(State::DISCONNECTING);
    esp_err_t err = driver_hal.disconnect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect wifi: %s", esp_err_to_name(err));
        state_machine.transition_to(state);
        sync_manager.set_bits(wifi_manager::CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_event(const Message &msg, State state)
{
    EventOutcome outcome = state_machine.resolve_event(msg.event);

    // 1. Perform state transition
    if (outcome.next_state != state) {
        ESP_LOGD(TAG, "Event %d: State transition %d -> %d", (int)msg.event, (int)state, (int)outcome.next_state);
        state_machine.transition_to(outcome.next_state);
    }

    // 2. Set synchronization bits for API callers
    if (outcome.bits_to_set != 0) {
        sync_manager.set_bits(outcome.bits_to_set);
    }

    // 3. Handle Side Effects (Complex logic)
    switch (msg.event) {
    case EventId::STA_DISCONNECTED:
    {
        const char *quality = (msg.rssi >= WiFiStateMachine::RSSI_THRESHOLD_GOOD)     ? "GOOD"
                              : (msg.rssi >= WiFiStateMachine::RSSI_THRESHOLD_MEDIUM) ? "MEDIUM"
                              : (msg.rssi >= WiFiStateMachine::RSSI_THRESHOLD_WEAK)   ? "WEAK"
                                                                                      : "CRITICAL";

        ESP_LOGI(TAG, "Task Event: STA_DISCONNECTED (reason: %d, RSSI=%d dBm [%s])", msg.reason, msg.rssi, quality);

        // Case A: Disconnection was intended or while driver is inactive
        if (state == State::DISCONNECTING || state == State::STOPPING || !state_machine.is_active()) {
            sync_manager.set_bits(wifi_manager::DISCONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT);
            break;
        }

        // Case B: Intentional disconnect from AP side (usually leave)
        if (msg.reason == WIFI_REASON_ASSOC_LEAVE) {
            ESP_LOGI(TAG, "Disconnected (Reason: ASSOC_LEAVE).");
            state_machine.transition_to(State::DISCONNECTED);
            sync_manager.set_bits(wifi_manager::DISCONNECTED_BIT | wifi_manager::CONNECT_FAILED_BIT);
            break;
        }

        // Case C: Definite credential failure (Currently NONE, all moved to Suspect to be RSSI-aware)
        // We could keep some here if we were sure they are NEVER caused by bad signal.

        // Case D: Suspect failure (potential wrong password or bad signal)
        // These reasons can be caused by both wrong credentials and poor signal/interference.
        // We handle this in a dynamic way based on RSSI, passing rssi to handle_suspect_failure()
        if (msg.reason == WIFI_REASON_AUTH_FAIL || msg.reason == WIFI_REASON_802_1X_AUTH_FAILED ||
            msg.reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || msg.reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
            msg.reason == WIFI_REASON_CONNECTION_FAIL) {
            if (state_machine.handle_suspect_failure(msg.rssi)) {
                ESP_LOGE(TAG, "Authentication failed due to too many suspect failures (Reason: %d). Invalidating.",
                         msg.reason);
                this->storage.save_valid_flag(false);
                // State machine already transited to ERROR_CREDENTIALS in handle_suspect_failure
            }
            else {
                uint32_t delay_ms;
                state_machine.calculate_next_backoff(delay_ms);
                ESP_LOGW(TAG,
                         "Suspect failure (Reason: %d), retrying in %lu ms due to poor signal or allowed attempts...",
                         msg.reason, (unsigned long)delay_ms);
                // State machine already transited to WAITING_RECONNECT in calculate_next_backoff
            }
            sync_manager.set_bits(wifi_manager::CONNECT_FAILED_BIT);
            break;
        }
        // Case E: Recoverable failure (signal loss, congestion, etc.)
        if (this->storage.is_valid()) {
            uint32_t delay_ms;
            state_machine.calculate_next_backoff(delay_ms);
            ESP_LOGI(TAG, "Reconnection attempt %lu in %lu ms...", (unsigned long)state_machine.get_retry_count(),
                     (unsigned long)delay_ms);
        }
        else {
            state_machine.transition_to(State::DISCONNECTED);
        }
        sync_manager.set_bits(wifi_manager::CONNECT_FAILED_BIT);
        break;
    }

    case EventId::GOT_IP:
        ESP_LOGI(TAG, "Task Event: GOT_IP");
        state_machine.reset_retries();
        if (!this->storage.is_valid()) {
            this->storage.save_valid_flag(true);
        }
        break;

    default:
        break;
    }
}

void WiFiManager::wifi_task(void *pvParameters)
{
    WiFiManager *self = static_cast<WiFiManager *>(pvParameters);
    Message msg;

    while (true) {
        // Ask the state machine how long to wait (it handles all backoff logic internally)
        TickType_t wait_ticks = self->state_machine.get_wait_ticks();

        if (xQueueReceive(self->sync_manager.get_queue(), &msg, wait_ticks) == pdTRUE) {
            xSemaphoreTakeRecursive(self->state_mutex, portMAX_DELAY);

            // Handle Task Termination
            if (msg.type == MessageType::COMMAND && msg.cmd == CommandId::EXIT) {
                ESP_LOGI(TAG, "WiFi Task exiting...");
                xSemaphoreGiveRecursive(self->state_mutex);
                self->task_handle = nullptr;
                vTaskDelete(NULL);
                return;
            }

            self->process_message(msg, self->state_machine.get_current_state());
            xSemaphoreGiveRecursive(self->state_mutex);
        }
        else {
            // Reconnect Backoff Timeout
            xSemaphoreTakeRecursive(self->state_mutex, portMAX_DELAY);
            if (self->state_machine.get_current_state() == State::WAITING_RECONNECT) {
                if (self->storage.is_valid()) {
                    ESP_LOGI(TAG, "Backoff finished. Retrying connection...");
                    self->state_machine.transition_to(State::CONNECTING);
                    self->driver_hal.connect();
                }
                else {
                    self->state_machine.transition_to(State::DISCONNECTED);
                }
            }
            xSemaphoreGiveRecursive(self->state_mutex);
        }
    }
}
