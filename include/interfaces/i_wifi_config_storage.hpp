#pragma once

#include "esp_err.h"
#include <string>

namespace wifi_manager {

/**
 * @class IWiFiConfigStorage
 * @brief Interface for handling persistence of WiFi credentials and validity flags.
 */
class IWiFiConfigStorage
{
public:
    virtual ~IWiFiConfigStorage() = default;

    /**
     * @brief Initialize NVS if not already initialized.
     * @return ESP_OK on success.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Add WiFi credentials to the storage, sync to driver and persist validity flag.
     * @param ssid WiFi SSID.
     * @param password WiFi password.
     * @return ESP_OK on success.
     */
    virtual esp_err_t add_credentials(const std::string &ssid, const std::string &password) = 0;

    /**
     * @brief Load WiFi credentials from the driver.
     * @param ssid [out] Loaded SSID.
     * @param password [out] Loaded password.
     * @return ESP_OK on success.
     */
    virtual esp_err_t load_credentials(std::string &ssid, std::string &password) = 0;

    /**
     * @brief Clear WiFi credentials from the driver and reset validity flag.
     * @return ESP_OK on success.
     */
    virtual esp_err_t clear_credentials() = 0;

    /**
     * @brief Reset all WiFi settings to factory defaults.
     * @return ESP_OK on success.
     */
    virtual esp_err_t factory_reset() = 0;

    /**
     * @brief Check if the stored credentials are considered valid.
     * @return true if valid.
     */
    virtual bool is_valid() const = 0;

    /**
     * @brief Save the validity flag to storage.
     * @param valid Validity status.
     * @return ESP_OK on success.
     */
    virtual esp_err_t save_valid_flag(bool valid) = 0;

    /**
     * @brief Ensure driver has a configuration, fallback to Kconfig if empty.
     * @return ESP_OK on success.
     */
    virtual esp_err_t ensure_config_fallback() = 0;
};

} // namespace wifi_manager
