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

/**
 * @file wifi_manager.hpp
 * @brief Concrete implementation of IWiFiManager.
 */

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
     *
     * @param driver_hal Pointer to driver HAL.
     * @param storage Pointer to config storage.
     * @param sync_manager Pointer to sync manager.
     * @param state_machine Pointer to state machine.
     * @param bootstrapper Pointer to bootstrapper.
     * @param processor Pointer to message processor.
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

    /**
     * @copydoc IWiFiManager::init()
     */
    esp_err_t init() override;

    /**
     * @copydoc IWiFiManager::deinit()
     */
    esp_err_t deinit() override;

    /**
     * @copydoc IWiFiManager::start(uint32_t)
     */
    esp_err_t start(uint32_t timeout_ms) override;

    /**
     * @copydoc IWiFiManager::start()
     */
    esp_err_t start() override;

    /**
     * @copydoc IWiFiManager::stop(uint32_t)
     */
    esp_err_t stop(uint32_t timeout_ms) override;

    /**
     * @copydoc IWiFiManager::stop()
     */
    esp_err_t stop() override;

    /**
     * @copydoc IWiFiManager::connect(uint32_t)
     */
    esp_err_t connect(uint32_t timeout_ms) override;

    /**
     * @copydoc IWiFiManager::connect()
     */
    esp_err_t connect() override;

    /**
     * @copydoc IWiFiManager::disconnect(uint32_t)
     */
    esp_err_t disconnect(uint32_t timeout_ms) override;

    /**
     * @copydoc IWiFiManager::disconnect()
     */
    esp_err_t disconnect() override;

    /**
     * @copydoc IWiFiManager::get_state()
     */
    State get_state() const override;

    /**
     * @copydoc IWiFiManager::add_credentials()
     */
    esp_err_t add_credentials(const std::string &ssid, const std::string &password) override;

    /**
     * @copydoc IWiFiManager::get_credentials()
     */
    esp_err_t get_credentials(std::string &ssid, std::string &password) override;

    /**
     * @copydoc IWiFiManager::clear_credentials()
     */
    esp_err_t clear_credentials() override;

    /**
     * @copydoc IWiFiManager::factory_reset()
     */
    esp_err_t factory_reset() override;

    /**
     * @copydoc IWiFiManager::is_credentials_valid()
     */
    bool is_credentials_valid() const override;

    /**
     * @copydoc IWiFiManager::get_task_handle()
     */
    TaskHandle_t get_task_handle() const override;

private:
    /**
     * @brief Internal helper to initialize NVS flash partition.
     * @return ESP_OK on success.
     */
    esp_err_t init_nvs();

    /**
     * @brief Helper to persist validity flag.
     * @param valid Validity status.
     * @return ESP_OK on success.
     */
    esp_err_t save_valid_flag(bool valid);

    /**
     * @brief Main FreeRTOS task loop that executes driver operations.
     * @param pvParameters Pointer to WiFiManager instance.
     */
    static void wifi_task(void *pvParameters);

    // Allow Bootstrapper to call the task trampoline
    friend class WiFiBootstrapper;

    /**
     * @brief Private helper to post messages to the internal queue.
     * @param msg Message to post.
     * @param is_async True if posting from an async context.
     * @return ESP_OK on success.
     */
    esp_err_t post_message(const Message &msg, bool is_async);

    // --- Sub-components (Interfaces) ---
    std::unique_ptr<IWiFiConfigStorage> storage_;      ///< Pointer to config storage
    std::unique_ptr<IWiFiStateMachine> state_machine_; ///< Pointer to state machine
    std::unique_ptr<IWiFiDriverHAL> driver_hal_;       ///< Pointer to driver HAL
    std::unique_ptr<IWiFiSyncManager> sync_manager_;   ///< Pointer to sync manager
    std::unique_ptr<IWiFiBootstrapper> bootstrapper_;   ///< Pointer to bootstrapper
    std::unique_ptr<IWiFiMessageProcessor> processor_; ///< Pointer to message processor

    // --- Private Members ---
    TaskHandle_t task_handle_;              ///< Task handling internal state
    mutable SemaphoreHandle_t state_mutex_; ///< Recursive mutex for thread-safe state access
};

} // namespace wifi_manager
