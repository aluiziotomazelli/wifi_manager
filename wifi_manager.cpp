#include <cstring>

#include "esp_event.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_manager.hpp"

static const char *TAG         = "WiFiManager";
constexpr int8_t RSSI_CRITICAL = -85;
constexpr int8_t RSSI_GOOD     = -65;

// =================================================================================================
// Singleton and Constructor/Destructor
// =================================================================================================

WiFiManager &WiFiManager::get_instance()
{
    static WiFiManager instance;
    return instance;
}

WiFiManager::WiFiManager()
    : wifi_event_instance(nullptr)
    , ip_event_instance(nullptr)
    , sta_netif(nullptr)
    , task_handle(nullptr)
    , command_queue(nullptr)
    , wifi_event_group(nullptr)
    , current_state(State::UNINITIALIZED)
    , is_credential_valid(false)
    , retry_count(0)
    , suspect_retry_count(0)
    , next_reconnect_ms(0)
{
    // Mutex is created once and persists for the lifetime of the singleton.
    // We use a recursive mutex because the wifi_task holds it while calling
    // internal methods (like save_valid_flag) that also need it.
    state_mutex = xSemaphoreCreateRecursiveMutex();
}

WiFiManager::~WiFiManager()
{
    // Cleanup if the object is ever destroyed (singleton usually lasts until shutdown)
    if (task_handle != nullptr) {
        vTaskDelete(task_handle);
    }
    if (command_queue != nullptr) {
        vQueueDelete(command_queue);
    }
    if (wifi_event_group != nullptr) {
        vEventGroupDelete(wifi_event_group);
    }
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
    if (current_state != State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        ESP_LOGI(TAG, "Already initialized or initializing.");
        return ESP_OK;
    }
    // Set to INITIALIZING to prevent concurrent init calls
    current_state = State::INITIALIZING;
    xSemaphoreGiveRecursive(state_mutex);

    // Global NVS init - not rolled back by deinit() as it's shared across the system
    esp_err_t err = init_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Global Netif init - allowed to fail if already initialized by another component
    err = esp_netif_init();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Netif initialized.");
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_netif_init: %s", esp_err_to_name(err));
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Netif already initialized.");
    }

    // Default event loop is shared - we don't delete it in deinit() to avoid breaking others
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        deinit(); // Component-specific setup failed, clean up allocated members
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop already created.");
    }

    // Check if the WiFi station interface already exists (idempotency)
    sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != nullptr) {
        ESP_LOGW(TAG, "Using existing default STA netif");
    }
    if (sta_netif == nullptr) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (sta_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create default STA netif");
        deinit();
        return ESP_FAIL;
    }

    // Initialize the WiFi driver stack
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err                    = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_wifi_init: %s", esp_err_to_name(err));
        deinit();
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "WiFi stack already initialized.");
    }

    // Ensure driver is in STA mode so we can read/write configs
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(err));
        deinit();
        return err;
    }

    // Register event handlers with instance pointers for the static callbacks
    err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::wifi_event_handler, this, &wifi_event_instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WIFI handler: %s", esp_err_to_name(err));
        deinit();
        return err;
    }

    err = esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::ip_event_handler, this, &ip_event_instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP handler: %s", esp_err_to_name(err));
        deinit();
        return err;
    }

    // RTOS resources for communication between the API and the internal task
    command_queue    = xQueueCreate(10, sizeof(Command));
    wifi_event_group = xEventGroupCreate();
    if (!command_queue || !wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create queue or event group");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    // Load validity flag from NVS
    nvs_handle_t h;
    err = nvs_open("wifi_manager", NVS_READONLY, &h);
    if (err == ESP_OK) {
        uint8_t valid = 0;
        if (nvs_get_u8(h, "valid", &valid) == ESP_OK) {
            is_credential_valid = (valid != 0);
        }
        nvs_close(h);
    }
    else {
        // If it doesn't exist, we'll determine it after checking driver config
        is_credential_valid = false;
    }

    // Check if the driver already has configuration
    wifi_config_t current_conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &current_conf) == ESP_OK) {
        if (strlen((char *)current_conf.sta.ssid) == 0) {
            // No SSID in driver, check Kconfig
            if (strlen(CONFIG_WIFI_SSID) > 0) {
                ESP_LOGI(TAG, "No SSID in driver, using Kconfig default: %s", CONFIG_WIFI_SSID);
                wifi_config_t wifi_config = {};
                // Use memcpy for SSID to support 32 characters correctly
                size_t ssid_len = strlen(CONFIG_WIFI_SSID);
                if (ssid_len > 32)
                    ssid_len = 32;
                memcpy(wifi_config.sta.ssid, CONFIG_WIFI_SSID, ssid_len);

                size_t pass_len = strlen(CONFIG_WIFI_PASSWORD);
                if (pass_len > 64)
                    pass_len = 64;
                memcpy(wifi_config.sta.password, CONFIG_WIFI_PASSWORD, pass_len);

                wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
                wifi_config.sta.failure_retry_cnt  = 2;
                wifi_config.sta.pmf_cfg.capable    = true;
                wifi_config.sta.pmf_cfg.required   = false;
                wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                is_credential_valid = true;
                save_valid_flag(true);
            }
        }
        else {
            ESP_LOGI(TAG, "Driver already has SSID: %s", current_conf.sta.ssid);
            // If we didn't find the flag in NVS but driver has SSID, assume valid
            // unless we specifically failed before
            err = nvs_open("wifi_manager", NVS_READONLY, &h);
            if (err != ESP_OK) {
                is_credential_valid = true;
                save_valid_flag(true);
            }
        }
    }

    // Launch the consumer task that executes all driver operations
    BaseType_t task_created = xTaskCreate(wifi_task, "wifi_task", 4096, this, 5, &task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi task");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    current_state = State::INITIALIZED;
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
    if ((uint32_t)state & IS_ACTIVE_MASK) {
        ESP_LOGI(TAG, "WiFi is running, stopping first...");
        stop(2000);
    }

    // 2. Terminate the manager task gracefully using the EXIT command
    if (task_handle != nullptr) {
        ESP_LOGI(TAG, "Stopping WiFi task...");
        Command cmd = {};
        cmd.id      = CommandId::EXIT;
        if (command_queue != nullptr &&
            xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
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

    // 3. Deinit the driver stack
    esp_err_t ret = esp_wifi_deinit();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi stack deinitialized.");
    }
    else if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGI(TAG, "WiFi stack was already deinitialized.");
    }
    else {
        ESP_LOGW(TAG, "WiFi stack deinit failed: %s", esp_err_to_name(ret));
    }

    // 4. Specifically destroy the default wifi sta netif to allow reuse after re-init
    if (sta_netif != nullptr) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = nullptr;
    }

    // 5. Unregister event handlers to prevent calling deleted instance members
    if (wifi_event_instance != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance);
        wifi_event_instance = nullptr;
    }
    if (ip_event_instance != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_instance);
        ip_event_instance = nullptr;
    }

    // 6. Delete the default event loop (DISABLED: usually too global to delete)
    // esp_event_loop_delete_default();

    // 7. Clean up internal RTOS synchronization objects
    if (command_queue != nullptr) {
        vQueueDelete(command_queue);
        command_queue = nullptr;
    }
    if (wifi_event_group != nullptr) {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = nullptr;
    }

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    current_state = State::UNINITIALIZED;
    xSemaphoreGiveRecursive(state_mutex);

    ESP_LOGI(TAG, "WiFi Manager deinitialized.");
    return ESP_OK;
}

esp_err_t WiFiManager::start(uint32_t timeout_ms)
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::START, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!wifi_event_group || !command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to start WiFi (sync)...");

    // Idempotency: skip if already in an active/started state
    if (!((uint32_t)state & CAN_START_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::START;

    xEventGroupClearBits(wifi_event_group, STARTED_BIT | START_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = send_command(cmd, false);
    if (err != ESP_OK) {
        return err;
    }

    // Wait for the Task to set the success or failure bit
    EventBits_t bits =
        xEventGroupWaitBits(wifi_event_group, STARTED_BIT | START_FAILED_BIT | INVALID_STATE_BIT,
                            pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    if (bits & INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & STARTED_BIT) {
        return ESP_OK;
    }
    if (bits & START_FAILED_BIT) {
        return ESP_FAIL;
    }

    // Rollback: if we timed out waiting for the driver, try to stop it to reset state
    ESP_LOGW(TAG, "Start timed out, cancelling...");
    stop();
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::start()
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::START, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to start WiFi (async)...");

    // Idempotency: skip if already in an active/started state
    if (!((uint32_t)state & CAN_START_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::START;
    return send_command(cmd, true);
}

esp_err_t WiFiManager::stop(uint32_t timeout_ms)
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::STOP, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!wifi_event_group || !command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "API: Requesting to stop WiFi (sync)...");

    // Idempotency: skip if already stopped
    if (!((uint32_t)state & CAN_STOP_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::STOP;

    xEventGroupClearBits(wifi_event_group, STOPPED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = send_command(cmd, false);
    if (err != ESP_OK) {
        return err;
    }

    EventBits_t bits =
        xEventGroupWaitBits(wifi_event_group, STOPPED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT,
                            pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    if (bits & INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & STOPPED_BIT) {
        return ESP_OK;
    }
    if (bits & STOP_FAILED_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::stop()
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::STOP, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to stop WiFi (async)...");

    // Idempotency: skip if already stopped
    if (!((uint32_t)state & CAN_STOP_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::STOP;
    return send_command(cmd, true);
}

esp_err_t WiFiManager::connect(uint32_t timeout_ms)
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::CONNECT, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!wifi_event_group || !command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to connect (sync)...");

    // Idempotency: skip if already connecting or connected
    if (!((uint32_t)state & CAN_CONNECT_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::CONNECT;

    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = send_command(cmd, false);
    if (err != ESP_OK) {
        return err;
    }

    // Wait for either the GOT_IP event (SUCCESS) or a DISCONNECT/ERROR event (FAIL)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    if (bits & INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & CONNECTED_BIT) {
        return ESP_OK;
    }
    else if (bits & CONNECT_FAILED_BIT) {
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
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::CONNECT, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to connect (async)...");

    // Idempotency: skip if already connecting or connected
    if (!((uint32_t)state & CAN_CONNECT_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::CONNECT;
    return send_command(cmd, true);
}

esp_err_t WiFiManager::disconnect(uint32_t timeout_ms)
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::DISCONNECT, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!wifi_event_group || !command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to disconnect (sync)...");

    // Idempotency: skip if already disconnected
    if (!((uint32_t)state & CAN_DISCONNECT_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::DISCONNECT;

    xEventGroupClearBits(wifi_event_group,
                         DISCONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = send_command(cmd, false);
    if (err != ESP_OK) {
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group, DISCONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT, pdTRUE,
        pdFALSE, pdMS_TO_TICKS(timeout_ms));

    if (bits & INVALID_STATE_BIT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bits & DISCONNECTED_BIT) {
        return ESP_OK;
    }
    if (bits & CONNECT_FAILED_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::disconnect()
{
    State state          = get_state();
    esp_err_t validation = validate_command(CommandId::DISCONNECT, state);
    if (validation != ESP_OK) {
        return validation;
    }

    if (!command_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "API: Requesting to disconnect (async)...");

    // Idempotency: skip if already disconnected
    if (!((uint32_t)state & CAN_DISCONNECT_MASK)) {
        return ESP_OK;
    }
    Command cmd = {};
    cmd.id      = CommandId::DISCONNECT;
    return send_command(cmd, true);
}

WiFiManager::State WiFiManager::get_state() const
{
    // The Mutex ensures that we don't read the state while the Task is mid-transition
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    State state = current_state;
    xSemaphoreGiveRecursive(state_mutex);
    return state;
}

// =================================================================================================
// Credentials and Reset
// =================================================================================================

esp_err_t WiFiManager::set_credentials(const std::string &ssid, const std::string &password)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (current_state == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "API: Setting credentials...");

    // If we are currently active, we must stop the current connection first
    if ((uint32_t)current_state & IS_ACTIVE_MASK) {
        ESP_LOGI(TAG, "Disconnecting before applying new credentials...");
        esp_wifi_disconnect();
    }

    wifi_config_t wifi_config = {};
    // Use memcpy for SSID and Password to support full range of characters
    size_t ssid_len = ssid.length() > 32 ? 32 : ssid.length();
    memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid_len);

    size_t pass_len = password.length() > 64 ? 64 : password.length();
    memcpy(wifi_config.sta.password, password.c_str(), pass_len);

    wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt  = 2;
    wifi_config.sta.pmf_cfg.capable    = true;
    wifi_config.sta.pmf_cfg.required   = false;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err == ESP_OK) {
        retry_count         = 0;
        suspect_retry_count = 0;
        save_valid_flag(true);
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
    wifi_config_t conf;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &conf);
    if (err == ESP_OK) {
        // SSID can be up to 32 chars and not null terminated
        char ssid_buf[33] = {0};
        memcpy(ssid_buf, conf.sta.ssid, 32);
        ssid = ssid_buf;

        // Password can be up to 64 chars and not null terminated
        char pass_buf[65] = {0};
        memcpy(pass_buf, conf.sta.password, 64);
        password = pass_buf;
    }
    return err;
}

esp_err_t WiFiManager::clear_credentials()
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (current_state == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "API: Clearing credentials...");

    wifi_config_t saved_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &saved_config);
    if (err != ESP_OK) {
        saved_config = {};
    }
    saved_config.sta.ssid[0]     = 0;
    saved_config.sta.password[0] = 0;

    err = esp_wifi_set_config(WIFI_IF_STA, &saved_config);
    if (err == ESP_OK) {
        retry_count         = 0;
        suspect_retry_count = 0;
        save_valid_flag(false);
    }
    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t WiFiManager::factory_reset()
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    if (current_state == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "API: Factory reset...");
    esp_wifi_restore();

    nvs_handle_t h;
    if (nvs_open("wifi_manager", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }

    is_credential_valid = false;
    retry_count         = 0;
    suspect_retry_count = 0;
    current_state       = State::INITIALIZED;

    xSemaphoreGiveRecursive(state_mutex);
    return ESP_OK;
}

bool WiFiManager::is_credentials_valid() const
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    bool valid = is_credential_valid;
    xSemaphoreGiveRecursive(state_mutex);
    return valid;
}

esp_err_t WiFiManager::save_valid_flag(bool valid)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("wifi_manager", NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    err = nvs_set_u8(h, "valid", valid ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
    is_credential_valid = valid;
    xSemaphoreGiveRecursive(state_mutex);

    return err;
}

// =================================================================================================
// Internal Implementation
// =================================================================================================

esp_err_t WiFiManager::send_command(const Command &cmd, bool is_async)
{
    // Basic safety check: don't even try to queue if we're not initialized
    if (get_state() == State::UNINITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Synchronous calls wait forever for a slot, async fail immediately if queue full
    TickType_t timeout = is_async ? 0 : portMAX_DELAY;
    if (xQueueSend(command_queue, &cmd, timeout) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send command to queue (full?)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void WiFiManager::process_command(const Command &cmd, State state)
{
    // Any explicit user command resets the retry counters
    if (cmd.id != CommandId::HANDLE_EVENT_WIFI && cmd.id != CommandId::HANDLE_EVENT_IP &&
        cmd.id != CommandId::EXIT) {
        this->retry_count         = 0;
        this->suspect_retry_count = 0;
    }

    switch (cmd.id) {
    case CommandId::START:
        handle_start(cmd, state);
        break;
    case CommandId::STOP:
        handle_stop(cmd, state);
        break;
    case CommandId::CONNECT:
        handle_connect(cmd, state);
        break;
    case CommandId::DISCONNECT:
        handle_disconnect(cmd, state);
        break;
    case CommandId::HANDLE_EVENT_WIFI:
        handle_wifi_event(cmd, state);
        break;
    case CommandId::HANDLE_EVENT_IP:
        handle_ip_event(cmd, state);
        break;
    default:
        break;
    }
}

void WiFiManager::handle_start(const Command &cmd, State state)
{
    if (this->validate_command(cmd.id, state) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot start WiFi in current state: %d", (int)state);
        xEventGroupSetBits(this->wifi_event_group, INVALID_STATE_BIT);
        return;
    }

    // If already operational or starting, just signal success for sync APIs
    if ((uint32_t)state & (IS_STA_READY_MASK | (uint32_t)State::STARTING)) {
        xEventGroupSetBits(this->wifi_event_group, STARTED_BIT);
        return;
    }

    // Normal transition: Valid states for start operation (INITIALIZED/STOPPED)
    this->current_state = State::STARTING;
    esp_err_t err       = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(err));
        this->current_state = state;
        xEventGroupSetBits(this->wifi_event_group, START_FAILED_BIT);
    }
}

void WiFiManager::handle_stop(const Command &cmd, State state)
{
    if (this->validate_command(cmd.id, state) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot stop WiFi in current state: %d", (int)state);
        xEventGroupSetBits(this->wifi_event_group, INVALID_STATE_BIT);
        return;
    }

    // If already stopped or stopping, signal success
    if ((uint32_t)state & ((uint32_t)State::STOPPED | (uint32_t)State::STOPPING)) {
        xEventGroupSetBits(this->wifi_event_group, STOPPED_BIT);
        return;
    }

    // Normal transition: Valid states for stop operation
    this->current_state = State::STOPPING;
    esp_err_t err       = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop wifi: %s", esp_err_to_name(err));
        this->current_state = state;
        xEventGroupSetBits(this->wifi_event_group, STOP_FAILED_BIT);
    }
}

void WiFiManager::handle_connect(const Command &cmd, State state)
{
    if (this->validate_command(cmd.id, state) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot connect in current state: %d", (int)state);
        xEventGroupSetBits(this->wifi_event_group, INVALID_STATE_BIT);
        return;
    }

    // If already connecting or connected, signal success
    if ((uint32_t)state & ((uint32_t)State::CONNECTING | IS_CONNECTED_MASK)) {
        if ((uint32_t)state & IS_CONNECTED_MASK) {
            xEventGroupSetBits(this->wifi_event_group, CONNECTED_BIT);
        }
        return;
    }

    // Normal transition: Valid states for connect operation
    this->current_state = State::CONNECTING;
    esp_err_t err       = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect wifi: %s", esp_err_to_name(err));
        this->current_state = state;
        xEventGroupSetBits(this->wifi_event_group, CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_disconnect(const Command &cmd, State state)
{
    if (this->validate_command(cmd.id, state) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot disconnect in current state: %d", (int)state);
        xEventGroupSetBits(this->wifi_event_group, INVALID_STATE_BIT);
        return;
    }

    // If already disconnected or disconnecting, signal success
    if ((uint32_t)state & ((uint32_t)State::DISCONNECTED | (uint32_t)State::DISCONNECTING)) {
        xEventGroupSetBits(this->wifi_event_group, DISCONNECTED_BIT);
        return;
    }

    // SPECIAL CASE: Rollback during early connect phase or backoff.
    if (state == State::WAITING_RECONNECT || state == State::CONNECTING) {
        this->current_state = State::DISCONNECTED;
        esp_wifi_disconnect();
        xEventGroupSetBits(this->wifi_event_group, DISCONNECTED_BIT);
        return;
    }

    // Normal transition: Valid states
    this->current_state = State::DISCONNECTING;
    esp_err_t err       = esp_wifi_disconnect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect wifi: %s", esp_err_to_name(err));
        this->current_state = state;
        xEventGroupSetBits(this->wifi_event_group, CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_wifi_event(const Command &cmd, State state)
{
    // Handle events from the WiFi driver and transition states accordingly
    switch (cmd.event_id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGD(TAG, "Task Event: STA_START");
        if (state == State::STARTING) {
            this->current_state = State::STARTED;
            xEventGroupSetBits(this->wifi_event_group, STARTED_BIT);
        }
        else {
            ESP_LOGW(TAG, "STA_START ignored in state %d", (int)state);
        }
        break;
    case WIFI_EVENT_STA_STOP:
        ESP_LOGD(TAG, "Task Event: STA_STOP");
        if (state == State::STOPPING) {
            this->current_state = State::STOPPED;
            xEventGroupSetBits(this->wifi_event_group, STOPPED_BIT);
        }
        else {
            ESP_LOGW(TAG, "STA_STOP ignored in state %d", (int)state);
        }
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGD(TAG, "Task Event: STA_CONNECTED");
        if (state == State::CONNECTING) {
            this->current_state = State::CONNECTED_NO_IP;
        }
        else {
            ESP_LOGW(TAG, "STA_CONNECTED ignored in state %d", (int)state);
        }
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
    {
        const char *quality =
            (cmd.rssi <= RSSI_CRITICAL) ? "CRITICAL" : (cmd.rssi >= RSSI_GOOD ? "GOOD" : "MEDIUM");
        ESP_LOGI(TAG, "Task Event: STA_DISCONNECTED (reason: %d, RSSI=%d dBm [%s])", cmd.reason,
                 cmd.rssi, quality);

        // Ignore if driver is not supposed to be running
        if (!((uint32_t)state & IS_ACTIVE_MASK)) {
            ESP_LOGW(TAG, "STA_DISCONNECTED ignored in state %d", (int)state);
            break;
        }

        // Case 1: Disconnection was intended (via API)
        if (state == State::DISCONNECTING || state == State::STOPPING) {
            if (state == State::DISCONNECTING) {
                this->current_state = State::DISCONNECTED;
            }
            // If STOPPING, we stay in STOPPING until WIFI_EVENT_STA_STOP
            xEventGroupSetBits(this->wifi_event_group, DISCONNECTED_BIT | CONNECT_FAILED_BIT);
        }
        // Case 1: Intentional disconnect
        else if (cmd.reason == WIFI_REASON_ASSOC_LEAVE) {
            ESP_LOGI(TAG, "Disconnected (Reason: ASSOC_LEAVE).");
            this->current_state = State::DISCONNECTED;
            xEventGroupSetBits(this->wifi_event_group, DISCONNECTED_BIT | CONNECT_FAILED_BIT);
        }
        // Case 2: Definite credential failure
        else if (cmd.reason == WIFI_REASON_AUTH_FAIL ||
                 cmd.reason == WIFI_REASON_802_1X_AUTH_FAILED ||
                 cmd.reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                 cmd.reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            ESP_LOGE(TAG, "Authentication failed (Reason: %d).", cmd.reason);
            this->current_state = State::ERROR_CREDENTIALS;
            this->save_valid_flag(false);
            xEventGroupSetBits(this->wifi_event_group, CONNECT_FAILED_BIT);
        }
        // Case 3: Suspect credential failure
        else if (cmd.reason == WIFI_REASON_CONNECTION_FAIL) {
            this->suspect_retry_count++;
            ESP_LOGW(TAG, "Suspect failure %lu/3 (Reason: %d).",
                     (unsigned long)this->suspect_retry_count, cmd.reason);

            if (this->suspect_retry_count >= 3) {
                ESP_LOGE(TAG, "Too many suspect failures. Invalidating credentials.");
                this->current_state = State::ERROR_CREDENTIALS;
                this->save_valid_flag(false);
            }
            else {
                this->current_state = State::WAITING_RECONNECT;
                this->retry_count++;
                uint32_t delay_ms = 1000 * (1 << (this->retry_count - 1));
                if (delay_ms > 300000)
                    delay_ms = 300000;
                this->next_reconnect_ms = (esp_timer_get_time() / 1000) + delay_ms;
            }
            xEventGroupSetBits(this->wifi_event_group, CONNECT_FAILED_BIT);
        }
        // Case 4: Recoverable failure
        else {
            if (this->is_credential_valid) {
                this->current_state = State::WAITING_RECONNECT;
                this->retry_count++;

                uint32_t delay_ms = 1000; // First retry after 1s
                if (this->retry_count > 1) {
                    // Exponential backoff: 2s, 4s, 8s... max 300s (5 min)
                    delay_ms = (1 << (this->retry_count - 1)) * 1000;
                    if (delay_ms > 300000)
                        delay_ms = 300000;
                }
                this->next_reconnect_ms = (esp_timer_get_time() / 1000) + delay_ms;

                ESP_LOGI(TAG, "Reconnection attempt %lu in %lu ms...",
                         (unsigned long)this->retry_count, (unsigned long)delay_ms);
            }
            else {
                ESP_LOGW(TAG, "Credentials invalid, not reconnecting.");
                this->current_state = State::DISCONNECTED;
            }

            // Signal failure so blocking connect() calls can return
            xEventGroupSetBits(this->wifi_event_group, CONNECT_FAILED_BIT);
        }
        break;
    }
    default:
        break;
    }
}

void WiFiManager::handle_ip_event(const Command &cmd, State state)
{
    if (cmd.event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Task Event: GOT_IP");
        if (state == State::CONNECTING || state == State::CONNECTED_NO_IP) {
            this->current_state       = State::CONNECTED_GOT_IP;
            this->retry_count         = 0; // Reset retries on success
            this->suspect_retry_count = 0;
            if (!this->is_credential_valid) {
                this->save_valid_flag(true);
            }
            xEventGroupSetBits(this->wifi_event_group, CONNECTED_BIT);
        }
        else {
            ESP_LOGW(TAG, "GOT_IP ignored in state %d", (int)state);
        }
    }
}

esp_err_t WiFiManager::validate_command(CommandId cmd, State current) const
{
    uint32_t s_bit = (uint32_t)current;

    switch (cmd) {
    case CommandId::START:
        if (s_bit & CAN_START_MASK)
            return ESP_OK;
        // Idempotency: Already operational or in an active state
        if (s_bit & (IS_ACTIVE_MASK | (uint32_t)State::STARTING))
            return ESP_OK;
        return ESP_ERR_INVALID_STATE;

    case CommandId::STOP:
        if (s_bit & CAN_STOP_MASK)
            return ESP_OK;
        // Idempotency: Already stopped or currently stopping
        if (s_bit & ((uint32_t)State::STOPPED | (uint32_t)State::STOPPING))
            return ESP_OK;
        return ESP_ERR_INVALID_STATE;

    case CommandId::CONNECT:
        if (s_bit & CAN_CONNECT_MASK)
            return ESP_OK;
        // Idempotency: Already connecting or connected
        if (s_bit & ((uint32_t)State::CONNECTING | IS_CONNECTED_MASK))
            return ESP_OK;
        return ESP_ERR_INVALID_STATE;

    case CommandId::DISCONNECT:
        if (s_bit & CAN_DISCONNECT_MASK)
            return ESP_OK;
        // Idempotency: Already disconnected or currently disconnecting
        if (s_bit & ((uint32_t)State::DISCONNECTED | (uint32_t)State::DISCONNECTING))
            return ESP_OK;
        return ESP_ERR_INVALID_STATE;

    default:
        // System events and internal commands are usually not gated here
        return ESP_OK;
    }
}

void WiFiManager::wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)

{
    // Bridge from the system event loop back to our managed task
    WiFiManager *self = static_cast<WiFiManager *>(arg);
    Command cmd       = {};
    cmd.id            = CommandId::HANDLE_EVENT_WIFI;
    cmd.event_id      = id;

    if (id == WIFI_EVENT_STA_DISCONNECTED && data != nullptr) {
        wifi_event_sta_disconnected_t *disconn = static_cast<wifi_event_sta_disconnected_t *>(data);
        cmd.reason                             = disconn->reason;
        cmd.rssi                               = disconn->rssi;
    }

    xQueueSendFromISR(self->command_queue, &cmd, nullptr);
}

void WiFiManager::ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    WiFiManager *self = static_cast<WiFiManager *>(arg);
    Command cmd       = {};
    cmd.id            = CommandId::HANDLE_EVENT_IP;
    cmd.event_id      = id;
    xQueueSendFromISR(self->command_queue, &cmd, nullptr);
}

void WiFiManager::wifi_task(void *pvParameters)
{
    WiFiManager *self = static_cast<WiFiManager *>(pvParameters);
    Command cmd;

    while (true) {
        TickType_t wait_ticks = portMAX_DELAY;

        // If we are waiting to reconnect, calculate the remaining backoff time
        if (self->current_state == State::WAITING_RECONNECT) {
            uint64_t now = esp_timer_get_time() / 1000;
            if (self->next_reconnect_ms > now) {
                wait_ticks = pdMS_TO_TICKS(self->next_reconnect_ms - now);
            }
            else {
                wait_ticks = 0; // Timer already expired
            }
        }

        // Wait for a command or a timeout (backoff expiration)
        if (xQueueReceive(self->command_queue, &cmd, wait_ticks) == pdTRUE) {
            // The state mutex is held for the duration of a command processing
            xSemaphoreTakeRecursive(self->state_mutex, portMAX_DELAY);

            if (cmd.id == CommandId::EXIT) {
                ESP_LOGI(TAG, "WiFi Task exiting...");
                xSemaphoreGiveRecursive(self->state_mutex);
                self->task_handle = nullptr; // Signal to deinit() that we are gone
                vTaskDelete(NULL);
                return; // Should not reach here
            }

            self->process_command(cmd, self->current_state);
            xSemaphoreGiveRecursive(self->state_mutex);
        }
        else {
            // Timeout reached in xQueueReceive - Backoff finished
            if (self->current_state == State::WAITING_RECONNECT) {
                xSemaphoreTakeRecursive(self->state_mutex, portMAX_DELAY);
                if (self->is_credential_valid) {
                    ESP_LOGI(TAG, "Retrying connection...");
                    self->current_state = State::CONNECTING;
                    esp_wifi_connect();
                }
                else {
                    self->current_state = State::DISCONNECTED;
                }
                xSemaphoreGiveRecursive(self->state_mutex);
            }
        }
    }
}

#ifdef UNIT_TEST

esp_err_t WiFiManager::test_helper_send_start_command(bool is_async)
{
    Command cmd = {};
    cmd.id      = CommandId::START;
    return send_command(cmd, is_async);
}

esp_err_t WiFiManager::test_helper_send_stop_command(bool is_async)
{
    Command cmd = {};
    cmd.id      = CommandId::STOP;
    return send_command(cmd, is_async);
}

esp_err_t WiFiManager::test_helper_send_connect_command(bool is_async)
{
    Command cmd = {};
    cmd.id      = CommandId::CONNECT;
    return send_command(cmd, is_async);
}

esp_err_t WiFiManager::test_helper_send_disconnect_command(bool is_async)
{
    Command cmd = {};
    cmd.id      = CommandId::DISCONNECT;
    return send_command(cmd, is_async);
}

uint32_t WiFiManager::test_helper_get_queue_pending_count() const
{
    if (!command_queue)
        return 0;
    return uxQueueMessagesWaiting(command_queue);
}

bool WiFiManager::test_helper_is_queue_full() const
{
    if (!command_queue)
        return true;
    return uxQueueSpacesAvailable(command_queue) == 0;
}
#endif // UNIT_TEST
