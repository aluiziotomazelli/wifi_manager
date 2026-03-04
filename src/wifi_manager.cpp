#include <cstring>
#include <memory>

#include "esp_event.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_event_handler.hpp"
#include "wifi_manager.hpp"

// Concrete implementations for the factory
#include "esp_timer_hal.hpp"
#include "wifi_config_storage.hpp"
#include "wifi_driver_hal.hpp"
#include "wifi_state_machine.hpp"
#include "wifi_sync_manager.hpp"

static const char *TAG = "WiFiManager";

namespace wifi_manager {

// =================================================================================================
// Singleton and Constructor/Destructor
// =================================================================================================

WiFiManager &WiFiManager::get_instance()
{
    static auto driver_hal = std::make_unique<WiFiDriverHAL>();
    static auto storage = std::make_unique<WiFiConfigStorage>(*driver_hal, "wifi_manager");
    static auto sync_manager = std::make_unique<WiFiSyncManager>();
    static auto timer_hal = std::make_unique<EspTimerHAL>();
    static auto state_machine = std::make_unique<WiFiStateMachine>(*timer_hal);

    static WiFiManager instance(
        std::move(driver_hal), std::move(storage), std::move(sync_manager), std::move(state_machine));
    return instance;
}

WiFiManager::WiFiManager(
    std::unique_ptr<IWiFiDriverHAL> driver_hal,
    std::unique_ptr<IWiFiConfigStorage> storage,
    std::unique_ptr<IWiFiSyncManager> sync_manager,
    std::unique_ptr<IWiFiStateMachine> state_machine)
    : storage_(std::move(storage))
    , state_machine_(std::move(state_machine))
    , driver_hal_(std::move(driver_hal))
    , sync_manager_(std::move(sync_manager))
{
    state_mutex_ = xSemaphoreCreateRecursiveMutex();
    task_handle_ = nullptr;
}

WiFiManager::~WiFiManager()
{
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
    }
    if (sync_manager_) {
        sync_manager_->deinit();
    }
    if (state_mutex_ != nullptr) {
        vSemaphoreDelete(state_mutex_);
    }
}

// =================================================================================================
// Public API
// =================================================================================================

esp_err_t WiFiManager::init()
{
    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    if (state_machine_->get_current_state() != State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex_);
        ESP_LOGI(TAG, "Already initialized or initializing.");
        return ESP_OK;
    }
    state_machine_->transition_to(State::INITIALIZING);
    xSemaphoreGiveRecursive(state_mutex_);

    esp_err_t err = storage_->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Storage/NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = driver_hal_->netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_netif_init: %s", esp_err_to_name(err));
        deinit();
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Netif already initialized.");
    }

    err = driver_hal_->event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        deinit();
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop already created.");
    }

    sta_netif_ = driver_hal_->netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif_ == nullptr) {
        sta_netif_ = driver_hal_->netif_create_default_wifi_sta();
    }
    if (sta_netif_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = driver_hal_->wifi_init(&cfg);
    if (err != ESP_OK) {
        deinit();
        ESP_LOGE(TAG, "Failed to esp_wifi_init: %s", esp_err_to_name(err));
        return err;
    }

    err = driver_hal_->wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    err = sync_manager_->init();
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    // Use member or static instance of WiFiEventHandler
    static WiFiEventHandler event_handler(sync_manager_.get());

    err = driver_hal_->event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler::wifi_event_callback, &event_handler, &wifi_event_instance_);
    if (err != ESP_OK) {
        deinit();
        return err;
    }
    err = driver_hal_->event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler::ip_event_callback, &event_handler, &ip_event_instance_);
    if (err != ESP_OK) {
        deinit();
        return err;
    }

    storage_->ensure_config_fallback();

    BaseType_t task_created = xTaskCreate(wifi_task, "wifi_task", 4096, this, 5, &task_handle_);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi task");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    state_machine_->transition_to(State::INITIALIZED);
    xSemaphoreGiveRecursive(state_mutex_);
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

    if (state_machine_->is_active()) {
        ESP_LOGI(TAG, "WiFi is running, stopping first...");
        stop(2000);
    }

    if (task_handle_ != nullptr) {
        ESP_LOGI(TAG, "Stopping WiFi task...");
        Message msg = {};
        msg.type = MessageType::COMMAND;
        msg.cmd = CommandId::EXIT;
        if (sync_manager_->is_initialized() && sync_manager_->post_message(msg) == ESP_OK) {
            int retry = 0;
            while (task_handle_ != nullptr && retry < 100) {
                vTaskDelay(pdMS_TO_TICKS(10));
                retry++;
            }
            if (task_handle_ == nullptr) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        if (task_handle_ != nullptr) {
            ESP_LOGW(TAG, "WiFi task did not exit gracefully, deleting...");
            vTaskDelete(task_handle_);
            task_handle_ = nullptr;
        }
        ESP_LOGI(TAG, "WiFi task terminated.");
    }

    esp_err_t ret = ESP_OK;
    esp_err_t err;

    if (wifi_event_instance_ != nullptr) {
        err = driver_hal_->event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance_);
        wifi_event_instance_ = nullptr;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unregister WiFi event handler: %s", esp_err_to_name(err));
            ret = err;
        }
    }
    if (ip_event_instance_ != nullptr) {
        err = driver_hal_->event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_instance_);
        ip_event_instance_ = nullptr;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unregister IP event handler: %s", esp_err_to_name(err));
            ret = err;
        }
    }

    err = driver_hal_->wifi_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize WiFi: %s", esp_err_to_name(err));
        ret = err;
    }

    driver_hal_->netif_destroy_default_wifi(sta_netif_);

    sync_manager_->deinit();

    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    state_machine_->transition_to(State::UNINITIALIZED);
    xSemaphoreGiveRecursive(state_mutex_);

    ESP_LOGI(TAG, "WiFi Manager deinitialized.");
    return ret;
}

esp_err_t WiFiManager::start(uint32_t timeout_ms)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;

    Action action = state_machine_->validate_command(CommandId::START);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;

    sync_manager_->clear_bits(STARTED_BIT | START_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK)
        return err;

    uint32_t bits = sync_manager_->wait_for_bits(STARTED_BIT | START_FAILED_BIT | INVALID_STATE_BIT, timeout_ms);

    if (bits & INVALID_STATE_BIT)
        return ESP_ERR_INVALID_STATE;
    if (bits & STARTED_BIT)
        return ESP_OK;
    if (bits & START_FAILED_BIT)
        return ESP_FAIL;

    stop();
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::start()
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::START);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    return post_message(msg, true);
}

esp_err_t WiFiManager::stop(uint32_t timeout_ms)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::STOP);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::STOP;

    sync_manager_->clear_bits(STOPPED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK)
        return err;

    uint32_t bits = sync_manager_->wait_for_bits(STOPPED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT, timeout_ms);

    if (bits & INVALID_STATE_BIT)
        return ESP_ERR_INVALID_STATE;
    if (bits & STOPPED_BIT)
        return ESP_OK;
    if (bits & STOP_FAILED_BIT)
        return ESP_FAIL;
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::stop()
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::STOP);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::STOP;
    return post_message(msg, true);
}

esp_err_t WiFiManager::connect(uint32_t timeout_ms)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::CONNECT);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::CONNECT;

    sync_manager_->clear_bits(CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK)
        return err;

    uint32_t bits = sync_manager_->wait_for_bits(CONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT, timeout_ms);

    if (bits & INVALID_STATE_BIT)
        return ESP_ERR_INVALID_STATE;
    if (bits & CONNECTED_BIT)
        return ESP_OK;
    else if (bits & CONNECT_FAILED_BIT)
        return ESP_FAIL;
    else {
        disconnect();
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t WiFiManager::connect()
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::CONNECT);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::CONNECT;
    return post_message(msg, true);
}

esp_err_t WiFiManager::disconnect(uint32_t timeout_ms)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::DISCONNECT);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::DISCONNECT;

    sync_manager_->clear_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT);
    esp_err_t err = post_message(msg, false);
    if (err != ESP_OK)
        return err;

    uint32_t bits = sync_manager_->wait_for_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT | INVALID_STATE_BIT, timeout_ms);

    if (bits & INVALID_STATE_BIT)
        return ESP_ERR_INVALID_STATE;
    if (bits & DISCONNECTED_BIT)
        return ESP_OK;
    if (bits & CONNECT_FAILED_BIT)
        return ESP_FAIL;
    return ESP_ERR_TIMEOUT;
}

esp_err_t WiFiManager::disconnect()
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    Action action = state_machine_->validate_command(CommandId::DISCONNECT);
    if (action == Action::ERROR)
        return ESP_ERR_INVALID_STATE;
    if (action == Action::SKIP)
        return ESP_OK;

    Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::DISCONNECT;
    return post_message(msg, true);
}

State WiFiManager::get_state() const
{
    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    State state = state_machine_->get_current_state();
    xSemaphoreGiveRecursive(state_mutex_);
    return state;
}

esp_err_t WiFiManager::set_credentials(const std::string &ssid, const std::string &password)
{
    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    if (state_machine_->get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex_);
        return ESP_ERR_INVALID_STATE;
    }

    if (state_machine_->is_active()) {
        driver_hal_->wifi_disconnect();
    }

    esp_err_t err = storage_->save_credentials(ssid, password);
    if (err == ESP_OK) {
        // state_machine_->reset_retries();
        // wifi_config_t cfg;
        // memset(&cfg, 0, sizeof(cfg));
        // strncpy((char *)cfg.sta.ssid, ssid.c_str(), sizeof(cfg.sta.ssid));
        // strncpy((char *)cfg.sta.password, password.c_str(), sizeof(cfg.sta.password));
        // driver_hal_->wifi_set_config(&cfg);
    }

    xSemaphoreGiveRecursive(state_mutex_);
    return err;
}

esp_err_t WiFiManager::get_credentials(std::string &ssid, std::string &password)
{
    return storage_->load_credentials(ssid, password);
}

esp_err_t WiFiManager::clear_credentials()
{
    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    if (state_machine_->get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex_);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = storage_->clear_credentials();
    if (err == ESP_OK) {
        state_machine_->reset_retries();
    }
    xSemaphoreGiveRecursive(state_mutex_);
    return err;
}

esp_err_t WiFiManager::factory_reset()
{
    xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    if (state_machine_->get_current_state() == State::UNINITIALIZED) {
        xSemaphoreGiveRecursive(state_mutex_);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = storage_->factory_reset();
    state_machine_->reset_retries();
    state_machine_->transition_to(State::INITIALIZED);
    xSemaphoreGiveRecursive(state_mutex_);
    return err;
}

bool WiFiManager::is_credentials_valid() const
{
    return storage_->is_valid();
}

esp_err_t WiFiManager::save_valid_flag(bool valid)
{
    return storage_->save_valid_flag(valid);
}

esp_err_t WiFiManager::post_message(const Message &msg, bool is_async)
{
    if (!sync_manager_->is_initialized())
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = sync_manager_->post_message(msg);
    if (err != ESP_OK && msg.type == MessageType::COMMAND) {
        ESP_LOGE(TAG, "Failed to send command to queue (full?)");
    }
    return err;
}

void WiFiManager::process_message(const Message &msg, State state)
{
    if (msg.type == MessageType::COMMAND) {
        if (msg.cmd != CommandId::EXIT) {
            state_machine_->reset_retries();
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
        handle_event(msg, state);
    }
}

void WiFiManager::handle_start(const Message &msg, State state)
{
    state_machine_->transition_to(State::STARTING);
    esp_err_t err = driver_hal_->wifi_start();
    if (err != ESP_OK) {
        state_machine_->transition_to(state);
        sync_manager_->set_bits(START_FAILED_BIT);
    }
}

void WiFiManager::handle_stop(const Message &msg, State state)
{
    state_machine_->transition_to(State::STOPPING);
    esp_err_t err = driver_hal_->wifi_stop();
    if (err != ESP_OK) {
        state_machine_->transition_to(state);
        sync_manager_->set_bits(STOP_FAILED_BIT);
    }
}

void WiFiManager::handle_connect(const Message &msg, State state)
{
    state_machine_->transition_to(State::CONNECTING);
    esp_err_t err = driver_hal_->wifi_connect();
    if (err != ESP_OK) {
        state_machine_->transition_to(state);
        sync_manager_->set_bits(CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_disconnect(const Message &msg, State state)
{
    if (state == State::WAITING_RECONNECT || state == State::CONNECTING) {
        state_machine_->transition_to(State::DISCONNECTED);
        driver_hal_->wifi_disconnect();
        sync_manager_->set_bits(DISCONNECTED_BIT);
        return;
    }
    state_machine_->transition_to(State::DISCONNECTING);
    esp_err_t err = driver_hal_->wifi_disconnect();
    if (err != ESP_OK) {
        state_machine_->transition_to(state);
        sync_manager_->set_bits(CONNECT_FAILED_BIT);
    }
}

void WiFiManager::handle_event(const Message &msg, State state)
{
    EventOutcome outcome = state_machine_->resolve_event(msg.event);

    if (outcome.next_state != state) {
        state_machine_->transition_to(outcome.next_state);
    }

    if (outcome.bits_to_set != 0) {
        sync_manager_->set_bits(outcome.bits_to_set);
    }

    switch (msg.event) {
    case EventId::STA_DISCONNECTED:
    {
        if (state == State::DISCONNECTING || state == State::STOPPING || !state_machine_->is_active()) {
            sync_manager_->set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT);
            break;
        }
        if (msg.reason == WIFI_REASON_ASSOC_LEAVE) {
            state_machine_->transition_to(State::DISCONNECTED);
            sync_manager_->set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT);
            break;
        }

        if (msg.reason == WIFI_REASON_AUTH_FAIL || msg.reason == WIFI_REASON_802_1X_AUTH_FAILED ||
            msg.reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || msg.reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
            msg.reason == WIFI_REASON_CONNECTION_FAIL) {
            if (state_machine_->handle_suspect_failure(msg.rssi)) {
                storage_->save_valid_flag(false);
            }
            else {
                uint32_t delay_ms;
                state_machine_->calculate_next_backoff(delay_ms);
            }
            sync_manager_->set_bits(CONNECT_FAILED_BIT);
            break;
        }
        if (storage_->is_valid()) {
            uint32_t delay_ms;
            state_machine_->calculate_next_backoff(delay_ms);
        }
        else {
            state_machine_->transition_to(State::DISCONNECTED);
        }
        sync_manager_->set_bits(CONNECT_FAILED_BIT);
        break;
    }
    case EventId::GOT_IP:
        state_machine_->reset_retries();
        if (!storage_->is_valid())
            storage_->save_valid_flag(true);
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
        uint32_t wait_ms = self->state_machine_->get_wait_ms();
        TickType_t wait_ticks = (wait_ms == 0xFFFFFFFF) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
        if (xQueueReceive(self->sync_manager_->get_queue(), &msg, wait_ticks) == pdTRUE) {
            xSemaphoreTakeRecursive(self->state_mutex_, portMAX_DELAY);
            if (msg.type == MessageType::COMMAND && msg.cmd == CommandId::EXIT) {
                xSemaphoreGiveRecursive(self->state_mutex_);
                self->task_handle_ = nullptr;
                vTaskDelete(NULL);
                return;
            }
            self->process_message(msg, self->state_machine_->get_current_state());
            xSemaphoreGiveRecursive(self->state_mutex_);
        }
        else {
            xSemaphoreTakeRecursive(self->state_mutex_, portMAX_DELAY);
            if (self->state_machine_->get_current_state() == State::WAITING_RECONNECT) {
                if (self->storage_->is_valid()) {
                    self->state_machine_->transition_to(State::CONNECTING);
                    self->driver_hal_->wifi_connect();
                }
                else {
                    self->state_machine_->transition_to(State::DISCONNECTED);
                }
            }
            xSemaphoreGiveRecursive(self->state_mutex_);
        }
    }
}

} // namespace wifi_manager
