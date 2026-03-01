#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "wifi_types.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_manager.hpp"
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
        std::unique_ptr<IWiFiStateMachine> state_machine);

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

    esp_err_t set_credentials(const std::string &ssid, const std::string &password) override;
    esp_err_t get_credentials(std::string &ssid, std::string &password) override;
    esp_err_t clear_credentials() override;
    esp_err_t factory_reset() override;
    bool is_credentials_valid() const override;

private:
    // Internal helper to initialize NVS flash partition
    esp_err_t init_nvs();

    // Helper to persist validity flag (DEPRECATED: used via storage)
    esp_err_t save_valid_flag(bool valid);

    // Main FreeRTOS task loop that executes driver operations
    static void wifi_task(void *pvParameters);

    // Private helper to post messages to the internal queue
    esp_err_t post_message(const Message &msg, bool is_async);

    // --- Sub-components (Interfaces) ---
    std::unique_ptr<IWiFiConfigStorage> storage_;
    std::unique_ptr<IWiFiStateMachine> state_machine_;
    std::unique_ptr<IWiFiDriverHAL> driver_hal_;
    std::unique_ptr<IWiFiSyncManager> sync_manager_;

    // --- Private Members ---
    TaskHandle_t task_handle_;              ///< Task handling internal state
    mutable SemaphoreHandle_t state_mutex_; ///< Recursive mutex for thread-safe state access

    esp_event_handler_instance_t wifi_event_instance_ = nullptr; ///< Instance for WiFi event handler
    esp_event_handler_instance_t ip_event_instance_ = nullptr;   ///< Instance for IP event handler
    esp_netif_t *sta_netif_ = nullptr;                           ///< Netif handle for STA interface

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

} // namespace wifi_manager
