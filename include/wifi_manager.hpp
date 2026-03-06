#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "wifi_types.hpp"
#include "interfaces/i_wifi_bootstrapper.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_manager.hpp"
#include "interfaces/i_wifi_message_processor.hpp"
#include "interfaces/i_wifi_state_machine.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

// Forward declaration for test accessor
class WiFiManagerTestAccessor;

namespace wifi_manager {

/**
 * @class WiFiManager
 * @brief Implementation of the WiFi Manager component.
 */
class WiFiManager : public IWiFiManager
{
    friend class ::WiFiManagerTestAccessor;

public:
    using State = IWiFiStateMachine::State;
    using CommandId = IWiFiStateMachine::CommandId;
    using EventId = IWiFiStateMachine::EventId;
    using Action = IWiFiStateMachine::Action;
    using EventOutcome = IWiFiStateMachine::EventOutcome;

    /**
     * @brief Get the singleton instance of WiFiManager.
     * @return Reference to the WiFiManager instance.
     */
    static WiFiManager &get_instance();

    /**
     * @brief Dependency injection constructor for testing.
     * @internal
     */
    WiFiManager(
        std::unique_ptr<IWiFiDriverHAL> driver_hal,
        std::unique_ptr<IWiFiConfigStorage> storage,
        std::unique_ptr<IWiFiSyncManager> sync_manager,
        std::unique_ptr<IWiFiStateMachine> state_machine,
        std::unique_ptr<IWiFiBootstrapper> bootstrapper,
        std::unique_ptr<IWiFiMessageProcessor> processor);

    // Prevent copying and assignment
    WiFiManager(const WiFiManager &) = delete;
    WiFiManager &operator=(const WiFiManager &) = delete;

    ~WiFiManager() override;

    // IWiFiManager implementation
    esp_err_t init() override;
    esp_err_t deinit() override;

    esp_err_t start(uint32_t timeout_ms) override;
    esp_err_t start() override;

    esp_err_t stop(uint32_t timeout_ms) override;
    esp_err_t stop() override;

    esp_err_t connect(uint32_t timeout_ms) override;
    esp_err_t connect() override;

    esp_err_t disconnect(uint32_t timeout_ms) override;
    esp_err_t disconnect() override;

    State get_state() const override;

    esp_err_t add_credentials(const std::string &ssid, const std::string &password) override;
    esp_err_t get_credentials(std::string &ssid, std::string &password) override;
    esp_err_t clear_credentials() override;
    esp_err_t factory_reset() override;
    bool is_credentials_valid() const override;

    TaskHandle_t get_task_handle() const override;

private:
    // Internal helper to initialize NVS flash partition
    esp_err_t init_nvs();

    // Helper to persist validity flag (DEPRECATED: used via storage)
    esp_err_t save_valid_flag(bool valid);

    // Main FreeRTOS task loop that executes driver operations
    static void wifi_task(void *pvParameters);

    // Allow Bootstrapper to call the task trampoline
    friend class WiFiBootstrapper;

    // Private helper to post messages to the internal queue
    esp_err_t post_message(const Message &msg, bool is_async);

    // --- Sub-components (Interfaces) ---
    std::unique_ptr<IWiFiConfigStorage> storage_;
    std::unique_ptr<IWiFiStateMachine> state_machine_;
    std::unique_ptr<IWiFiDriverHAL> driver_hal_;
    std::unique_ptr<IWiFiSyncManager> sync_manager_;
    std::unique_ptr<IWiFiBootstrapper> bootstrapper_;
    std::unique_ptr<IWiFiMessageProcessor> processor_;

    // --- Private Members ---
    TaskHandle_t task_handle_;              ///< Task handling internal state
    mutable SemaphoreHandle_t state_mutex_; ///< Recursive mutex for thread-safe state access
};

} // namespace wifi_manager
