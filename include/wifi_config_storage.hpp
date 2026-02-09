#pragma once

#include "esp_err.h"
#include <string>

class WiFiDriverHAL;

/**
 * @class WiFiConfigStorage
 * @brief Handles persistence of WiFi credentials and validity flags using NVS.
 */
class WiFiConfigStorage
{
public:
    /**
     * @brief Constructor.
     * @param hal Reference to the driver HAL.
     * @param nvs_namespace NVS namespace to use for storage.
     */
    explicit WiFiConfigStorage(WiFiDriverHAL &hal, const char *nvs_namespace = "wifi_manager");

    /**
     * @brief Initialize NVS if not already initialized.
     * @return ESP_OK on success.
     */
    esp_err_t init();

    /**
     * @brief Save WiFi credentials to the driver and persist validity flag.
     * @param ssid WiFi SSID.
     * @param password WiFi password.
     * @return ESP_OK on success.
     */
    esp_err_t save_credentials(const std::string &ssid, const std::string &password);

    /**
     * @brief Load WiFi credentials from the driver.
     * @param ssid [out] Loaded SSID.
     * @param password [out] Loaded password.
     * @return ESP_OK on success.
     */
    esp_err_t load_credentials(std::string &ssid, std::string &password);

    /**
     * @brief Clear WiFi credentials from the driver and reset validity flag.
     * @return ESP_OK on success.
     */
    esp_err_t clear_credentials();

    /**
     * @brief Reset all WiFi settings to factory defaults.
     * @return ESP_OK on success.
     */
    esp_err_t factory_reset();

    /**
     * @brief Check if the stored credentials are considered valid.
     * @return true if valid.
     */
    bool is_valid() const;

    /**
     * @brief Save the validity flag to NVS.
     * @param valid Validity status.
     * @return ESP_OK on success.
     */
    esp_err_t save_valid_flag(bool valid);

    /**
     * @brief Ensure driver has a configuration, fallback to Kconfig if empty.
     * @return ESP_OK on success.
     */
    esp_err_t ensure_config_fallback();

private:
    WiFiDriverHAL &m_hal;
    const char *m_nvs_namespace;
    bool m_is_valid;

    esp_err_t load_valid_flag();
};
