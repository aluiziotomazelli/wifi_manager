#pragma once

#include "esp_err.h"
#include <string>

/**
 * @file i_wifi_config_storage.hpp
 * @brief Interface for handling persistence of WiFi credentials and validity flags.
 */

namespace wifi_manager {

/**
 * @class IWiFiConfigStorage
 * @brief Interface for handling persistence of WiFi credentials and validity flags.
 * @internal
 */
class IWiFiConfigStorage
{
public:
    virtual ~IWiFiConfigStorage() = default;

    /**
     * @brief Initialize NVS if not already initialized.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Add WiFi credentials to the storage, sync to driver and persist validity flag.
     * @internal
     * @param ssid WiFi SSID.
     * @param password WiFi password.
     * @return
     *     - ESP_OK: Success.
     *     - ESP_ERR_INVALID_ARG: SSID or password length is too long.
     *     - Other: Error codes from NVS operations.
     */
    virtual esp_err_t add_credentials(const std::string &ssid, const std::string &password) = 0;

    /**
     * @brief Load WiFi credentials from the driver.
     * @internal
     * @param[out] ssid Loaded SSID.
     * @param[out] password Loaded password.
     * @return ESP_OK on success.
     */
    virtual esp_err_t load_credentials(std::string &ssid, std::string &password) = 0;

    /**
     * @brief Clear WiFi credentials from the driver and reset validity flag.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t clear_credentials() = 0;

    /**
     * @brief Reset all WiFi settings to factory defaults.
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t factory_reset() = 0;

    /**
     * @brief Check if the stored credentials are considered valid.
     * @internal
     * @return true if valid.
     */
    virtual bool is_valid() const = 0;

    /**
     * @brief Save the validity flag to storage.
     * @internal
     * @param valid Validity status.
     * @return ESP_OK on success.
     */
    virtual esp_err_t save_valid_flag(bool valid) = 0;
};

} // namespace wifi_manager
