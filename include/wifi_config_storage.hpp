#pragma once

#include "esp_err.h"
#include <string>

#include "interfaces/i_wifi_config_storage.hpp"

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
     * @brief Constructor.
     * @param hal Reference to the driver HAL interface.
     * @param nvs_namespace NVS namespace to use for storage.
     */
    explicit WiFiConfigStorage(IWiFiDriverHAL &hal, const char *nvs_namespace = "wifi_manager");

    /**
     * @brief Initialize NVS if not already initialized.
     * @return ESP_OK on success.
     */
    esp_err_t init() override;

    /**
     * @brief Add WiFi credentials to the storage, sync to driver and persist validity flag.
     * @param ssid WiFi SSID.
     * @param password WiFi password.
     * @return ESP_OK on success.
     */
    esp_err_t add_credentials(const std::string &ssid, const std::string &password) override;

    /**
     * @brief Load WiFi credentials from the driver.
     * @param ssid [out] Loaded SSID.
     * @param password [out] Loaded password.
     * @return ESP_OK on success.
     */
    esp_err_t load_credentials(std::string &ssid, std::string &password) override;

    /**
     * @brief Clear WiFi credentials from the driver and reset validity flag.
     * @return ESP_OK on success.
     */
    esp_err_t clear_credentials() override;

    /**
     * @brief Reset all WiFi settings to factory defaults.
     * @return ESP_OK on success.
     */
    esp_err_t factory_reset() override;

    /**
     * @brief Check if the stored credentials are considered valid.
     * @return true if valid.
     */
    bool is_valid() const override;

    /**
     * @brief Save the validity flag to storage.
     * @param valid Validity status.
     * @return ESP_OK on success.
     */
    esp_err_t save_valid_flag(bool valid) override;

private:
    IWiFiDriverHAL &hal_;
    const char *nvs_namespace_;
    bool is_valid_;

    esp_err_t load_valid_flag();
    esp_err_t sync_to_driver(const std::string &ssid, const std::string &password);
};

} // namespace wifi_manager
