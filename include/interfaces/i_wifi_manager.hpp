#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include <string>

/**
 * @file i_wifi_manager.hpp
 * @brief Public interface for the WiFi Manager component.
 */

namespace wifi_manager {

/**
 * @class IWiFiManager
 * @brief Interface for the WiFi Manager component.
 *
 * This class defines the high-level API for managing WiFi connectivity on ESP32.
 * It provides methods for initialization, starting/stopping the WiFi driver,
 * connecting to Access Points, and managing credentials.
 */
class IWiFiManager
{
public:
    virtual ~IWiFiManager() = default;

    /**
     * @brief Initialize the WiFi Manager component.
     *
     * This method prepares internal resources, initializes NVS, sets up the WiFi driver
     * in Station mode, and starts the background management task.
     *
     * @return
     *     - ESP_OK: Success or already initialized.
     *     - ESP_ERR_NO_MEM: Failed to allocate memory for internal synchronization objects or task.
     *     - Other: Error codes propagated from the underlying NVS or WiFi driver initialization.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitialize the WiFi Manager component.
     *
     * This method stops any active WiFi connection, terminates the background task,
     * and releases all allocated resources.
     *
     * @return
     *     - ESP_OK: Success or already deinitialized.
     *     - Other: Error codes propagated from the underlying HAL or FreeRTOS operations.
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Synchronously start the WiFi driver.
     *
     * Blocks until the WiFi driver is successfully started or the timeout occurs.
     *
     * @param timeout_ms Maximum time in milliseconds to wait for the operation to complete.
     *
     * @return
     *     - ESP_OK: WiFi started successfully.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in a state where start is not allowed.
     *     - ESP_ERR_TIMEOUT: Operation timed out.
     *     - ESP_FAIL: Internal command queue error or driver failure.
     */
    virtual esp_err_t start(uint32_t timeout_ms) = 0;

    /**
     * @brief Asynchronously start the WiFi driver.
     *
     * Queues a start command to the background task and returns immediately.
     *
     * @return
     *     - ESP_OK: Command successfully queued.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in a state where start is not allowed.
     *     - ESP_FAIL: Internal command queue is full.
     */
    virtual esp_err_t start() = 0;

    /**
     * @brief Synchronously stop the WiFi driver.
     *
     * Blocks until the WiFi driver is successfully stopped or the timeout occurs.
     *
     * @param timeout_ms Maximum time in milliseconds to wait for the operation to complete.
     *
     * @return
     *     - ESP_OK: WiFi stopped successfully.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in a state where stop is not allowed.
     *     - ESP_ERR_TIMEOUT: Operation timed out.
     *     - ESP_FAIL: Internal command queue error or driver failure.
     */
    virtual esp_err_t stop(uint32_t timeout_ms) = 0;

    /**
     * @brief Asynchronously stop the WiFi driver.
     *
     * Queues a stop command to the background task and returns immediately.
     *
     * @return
     *     - ESP_OK: Command successfully queued.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in a state where stop is not allowed.
     *     - ESP_FAIL: Internal command queue is full.
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Synchronously connect to the Access Point using stored credentials.
     *
     * Blocks until the connection is established and an IP address is obtained, or the timeout occurs.
     *
     * @param timeout_ms Maximum time in milliseconds to wait for the connection to complete.
     *
     * @return
     *     - ESP_OK: Successfully connected and obtained an IP address.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized, WiFi not started, or in an invalid state.
     *     - ESP_ERR_TIMEOUT: Operation timed out before obtaining an IP.
     *     - ESP_FAIL: Internal command queue error or connection failure.
     */
    virtual esp_err_t connect(uint32_t timeout_ms) = 0;

    /**
     * @brief Asynchronously connect to the Access Point using stored credentials.
     *
     * Queues a connect command to the background task and returns immediately.
     *
     * @return
     *     - ESP_OK: Command successfully queued.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in an invalid state.
     *     - ESP_FAIL: Internal command queue is full.
     */
    virtual esp_err_t connect() = 0;

    /**
     * @brief Synchronously disconnect from the current Access Point.
     *
     * Blocks until the disconnection is complete or the timeout occurs.
     *
     * @param timeout_ms Maximum time in milliseconds to wait for the operation to complete.
     *
     * @return
     *     - ESP_OK: Successfully disconnected.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or WiFi not connected.
     *     - ESP_ERR_TIMEOUT: Operation timed out.
     *     - ESP_FAIL: Internal command queue error or driver failure.
     */
    virtual esp_err_t disconnect(uint32_t timeout_ms) = 0;

    /**
     * @brief Asynchronously disconnect from the current Access Point.
     *
     * Queues a disconnect command to the background task and returns immediately.
     *
     * @return
     *     - ESP_OK: Command successfully queued.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized or in an invalid state.
     *     - ESP_FAIL: Internal command queue is full.
     */
    virtual esp_err_t disconnect() = 0;

    /**
     * @brief Get the current internal state of the WiFi Manager.
     *
     * @return The current state as a @ref State enum value.
     */
    virtual State get_state() const = 0;

    /**
     * @brief Add or update WiFi credentials in persistent storage.
     *
     * Saves the SSID and password to NVS. If the WiFi driver is currently active,
     * it will be disconnected to apply the new credentials.
     *
     * @param ssid The SSID of the Access Point.
     * @param password The password for the Access Point.
     *
     * @return
     *     - ESP_OK: Credentials successfully saved.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized.
     *     - ESP_ERR_INVALID_ARG: SSID or password length exceeds the maximum allowed.
     *     - Other: Error codes propagated from the underlying NVS operations.
     */
    virtual esp_err_t add_credentials(const std::string &ssid, const std::string &password) = 0;

    /**
     * @brief Retrieve the currently stored WiFi credentials from persistent storage.
     *
     * @param[out] ssid String to store the retrieved SSID.
     * @param[out] password String to store the retrieved password.
     *
     * @return
     *     - ESP_OK: Credentials successfully retrieved.
     *     - Other: Error codes propagated from the underlying NVS operations.
     */
    virtual esp_err_t get_credentials(std::string &ssid, std::string &password) = 0;

    /**
     * @brief Clear all stored WiFi credentials from persistent storage.
     *
     * Erases credentials from NVS and resets the internal retry counters.
     *
     * @return
     *     - ESP_OK: Credentials successfully cleared.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized.
     *     - Other: Error codes propagated from the underlying NVS operations.
     */
    virtual esp_err_t clear_credentials() = 0;

    /**
     * @brief Reset the WiFi Manager and the WiFi driver to factory defaults.
     *
     * Clears all credentials, wipes the NVS namespace, and restores WiFi driver settings.
     *
     * @return
     *     - ESP_OK: Factory reset successful.
     *     - ESP_ERR_INVALID_STATE: Manager not initialized.
     *     - Other: Error codes propagated from NVS or WiFi driver operations.
     */
    virtual esp_err_t factory_reset() = 0;

    /**
     * @brief Check if valid WiFi credentials are currently stored.
     *
     * @return
     *     - true: Valid credentials found in storage.
     *     - false: No valid credentials available.
     */
    virtual bool is_credentials_valid() const = 0;

    /**
     * @brief Get the FreeRTOS task handle for the background WiFi task.
     *
     * @return Handle to the internal WiFi task.
     */
    virtual TaskHandle_t get_task_handle() const = 0;
};

} // namespace wifi_manager
