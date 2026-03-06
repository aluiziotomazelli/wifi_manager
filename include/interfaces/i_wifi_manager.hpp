#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include <string>

namespace wifi_manager {

/**
 * @class IWiFiManager
 * @brief Interface for the WiFi Manager component.
 */
class IWiFiManager
{
public:
    virtual ~IWiFiManager() = default;

    virtual esp_err_t init() = 0;
    virtual esp_err_t deinit() = 0;

    virtual esp_err_t start(uint32_t timeout_ms) = 0;
    virtual esp_err_t start() = 0;

    virtual esp_err_t stop(uint32_t timeout_ms) = 0;
    virtual esp_err_t stop() = 0;

    virtual esp_err_t connect(uint32_t timeout_ms) = 0;
    virtual esp_err_t connect() = 0;

    virtual esp_err_t disconnect(uint32_t timeout_ms) = 0;
    virtual esp_err_t disconnect() = 0;

    virtual State get_state() const = 0;

    virtual esp_err_t set_credentials(const std::string &ssid, const std::string &password) = 0;
    virtual esp_err_t get_credentials(std::string &ssid, std::string &password) = 0;
    virtual esp_err_t clear_credentials() = 0;
    virtual esp_err_t factory_reset() = 0;
    virtual bool is_credentials_valid() const = 0;

    virtual TaskHandle_t get_task_handle() const = 0;
};

} // namespace wifi_manager
