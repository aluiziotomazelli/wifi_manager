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
#include "wifi_bootstrapper.hpp"
#include "wifi_config_storage.hpp"
#include "wifi_driver_hal.hpp"
#include "wifi_message_processor.hpp"
#include "wifi_state_machine.hpp"
#include "wifi_sync_manager.hpp"

static const char *TAG = "WiFiManager";

namespace wifi_manager {

// =================================================================================================
// Singleton and Constructor/Destructor
// =================================================================================================
// LCOV_EXCL_START
WiFiManager &WiFiManager::get_instance()
{
    static auto driver_hal = std::make_unique<WiFiDriverHAL>();
    static auto storage = std::make_unique<WiFiConfigStorage>(*driver_hal, "wifi_manager");
    static auto sync_manager = std::make_unique<WiFiSyncManager>();
    static auto timer_hal = std::make_unique<EspTimerHAL>();
    static auto state_machine = std::make_unique<WiFiStateMachine>(*timer_hal);
    static auto bootstrapper = std::make_unique<WiFiBootstrapper>(*driver_hal, *storage, *sync_manager);
    static auto processor =
        std::make_unique<WiFiMessageProcessor>(*driver_hal, *storage, *state_machine, *sync_manager);

    static WiFiManager instance(
        std::move(driver_hal),
        std::move(storage),
        std::move(sync_manager),
        std::move(state_machine),
        std::move(bootstrapper),
        std::move(processor));
    return instance;
}
// LCOV_EXCL_STOP

WiFiManager::WiFiManager(
    std::unique_ptr<IWiFiDriverHAL> driver_hal,
    std::unique_ptr<IWiFiConfigStorage> storage,
    std::unique_ptr<IWiFiSyncManager> sync_manager,
    std::unique_ptr<IWiFiStateMachine> state_machine,
    std::unique_ptr<IWiFiBootstrapper> bootstrapper,
    std::unique_ptr<IWiFiMessageProcessor> processor)
    : storage_(std::move(storage))
    , state_machine_(std::move(state_machine))
    , driver_hal_(std::move(driver_hal))
    , sync_manager_(std::move(sync_manager))
    , bootstrapper_(std::move(bootstrapper))
    , processor_(std::move(processor))
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

    // Pass wifi_task explicitly; WiFiBootstrapper is agnostic about which function it creates.
    esp_err_t err = bootstrapper_->init(WiFiManager::wifi_task, this, &task_handle_);
    if (err != ESP_OK) {
        xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
        state_machine_->transition_to(State::UNINITIALIZED);
        xSemaphoreGiveRecursive(state_mutex_);
        return err;
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

    esp_err_t ret = bootstrapper_->deinit(&task_handle_);

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

TaskHandle_t WiFiManager::get_task_handle() const
{
    return task_handle_;
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

// TODO: save_valid_flag should be removed? Is used now on WiFiConfigStoraged
esp_err_t WiFiManager::save_valid_flag(bool valid)
{
    return storage_->save_valid_flag(valid);
}

// TODO: WiFiManager only uses MessageType::COMMAND now, MessageType::COMMAND is used on
// WiFiMessageProcessor::process_message, should be refactored? enum class MessageType : uint8_t and struct Message on
// wifi_types.hpp

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
            self->processor_->process_message(msg, self->state_machine_->get_current_state());
            xSemaphoreGiveRecursive(self->state_mutex_);
        }
        else {
            xSemaphoreTakeRecursive(self->state_mutex_, portMAX_DELAY);
            self->processor_->on_idle_tick(self->state_machine_->get_current_state());
            xSemaphoreGiveRecursive(self->state_mutex_);
        }
    }
}

} // namespace wifi_manager
