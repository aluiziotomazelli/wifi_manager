#pragma once

#include "esp_err.h"
#include <string>

#include "interfaces/i_wifi_config_storage.hpp"

/**
 * @file wifi_config_storage.hpp
 * @brief Concrete implementation of IWiFiConfigStorage using NVS.
 */

namespace wifi_manager {

class IWiFiDriverHAL;

/**
 * @class WiFiConfigStorage
 * @brief Handles persistence of WiFi credentials and validity flags using NVS.
 */
class WiFiConfigStorage : public IWiFiConfigStorage
{
public:
    /**
     * @brief Maximum number of WiFi networks that can be stored in NVS.
     */
    static constexpr uint8_t MAX_AP_COUNT = 10;

    /**
     * @brief Constructor for WiFiConfigStorage.
     * @param hal Reference to the driver HAL interface.
     * @param nvs_namespace NVS namespace to use for storage.
     */
    explicit WiFiConfigStorage(IWiFiDriverHAL &hal, const char *nvs_namespace = "wifi_manager");

    /**
     * @copydoc IWiFiConfigStorage::init()
     */
    esp_err_t init() override;

    /**
     * @copydoc IWiFiConfigStorage::add_credentials()
     */
    esp_err_t add_credentials(const std::string &ssid, const std::string &password) override;

    /**
     * @copydoc IWiFiConfigStorage::load_credentials()
     */
    esp_err_t load_credentials(std::string &ssid, std::string &password) override;

    /**
     * @copydoc IWiFiConfigStorage::clear_credentials()
     */
    esp_err_t clear_credentials() override;

    /**
     * @copydoc IWiFiConfigStorage::factory_reset()
     */
    esp_err_t factory_reset() override;

    /**
     * @copydoc IWiFiConfigStorage::is_valid()
     */
    bool is_valid() const override;

    /**
     * @copydoc IWiFiConfigStorage::save_valid_flag()
     */
    esp_err_t save_valid_flag(bool valid) override;

private:
    IWiFiDriverHAL &hal_;       ///< Reference to the WiFi driver HAL
    const char *nvs_namespace_; ///< NVS namespace for storage
    bool is_valid_;             ///< Flag indicating if credentials are valid

    /**
     * @brief Load the validity flag from NVS.
     * @return ESP_OK on success.
     */
    esp_err_t load_valid_flag();

    /**
     * @brief Sync the provided credentials to the WiFi driver.
     * @param ssid WiFi SSID.
     * @param password WiFi password.
     * @return ESP_OK on success.
     */
    esp_err_t sync_to_driver(const std::string &ssid, const std::string &password);
};

} // namespace wifi_manager
