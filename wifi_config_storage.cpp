#include "wifi_config_storage.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi_driver_hal.hpp"
#include <cstring>

static const char *TAG = "WiFiConfigStorage";

WiFiConfigStorage::WiFiConfigStorage(WiFiDriverHAL &hal, const char *nvs_namespace)
    : m_hal(hal)
    , m_nvs_namespace(nvs_namespace)
    , m_is_valid(false)
{
}

esp_err_t WiFiConfigStorage::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    return load_valid_flag();
}

esp_err_t WiFiConfigStorage::save_credentials(const std::string &ssid, const std::string &password)
{
    wifi_config_t wifi_config = {};
    size_t ssid_len           = ssid.length() > 32 ? 32 : ssid.length();
    memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid_len);

    size_t pass_len = password.length() > 64 ? 64 : password.length();
    memcpy(wifi_config.sta.password, password.c_str(), pass_len);

    wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt  = 0;
    wifi_config.sta.pmf_cfg.capable    = true;
    wifi_config.sta.pmf_cfg.required   = false;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t err = m_hal.set_config(&wifi_config);
    if (err == ESP_OK) {
        return save_valid_flag(true);
    }
    return err;
}

esp_err_t WiFiConfigStorage::load_credentials(std::string &ssid, std::string &password)
{
    wifi_config_t conf;
    esp_err_t err = m_hal.get_config(&conf);
    if (err == ESP_OK) {
        char ssid_buf[33] = {0};
        memcpy(ssid_buf, conf.sta.ssid, 32);
        ssid = ssid_buf;

        char pass_buf[65] = {0};
        memcpy(pass_buf, conf.sta.password, 64);
        password = pass_buf;
    }
    return err;
}

esp_err_t WiFiConfigStorage::clear_credentials()
{
    wifi_config_t saved_config;
    esp_err_t err = m_hal.get_config(&saved_config);
    if (err != ESP_OK) {
        saved_config = {};
    }
    saved_config.sta.ssid[0]     = 0;
    saved_config.sta.password[0] = 0;

    err = m_hal.set_config(&saved_config);
    if (err == ESP_OK) {
        return save_valid_flag(false);
    }
    return err;
}

esp_err_t WiFiConfigStorage::factory_reset()
{
    m_hal.restore();

    nvs_handle_t h;
    if (nvs_open(m_nvs_namespace, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }

    m_is_valid = false;
    return ESP_OK;
}

bool WiFiConfigStorage::is_valid() const
{
    return m_is_valid;
}

esp_err_t WiFiConfigStorage::save_valid_flag(bool valid)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(m_nvs_namespace, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(h, "valid", valid ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        m_is_valid = valid;
    }

    return err;
}

esp_err_t WiFiConfigStorage::load_valid_flag()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(m_nvs_namespace, NVS_READONLY, &h);
    if (err == ESP_OK) {
        uint8_t valid = 0;
        if (nvs_get_u8(h, "valid", &valid) == ESP_OK) {
            m_is_valid = (valid != 0);
        }
        nvs_close(h);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND) {
        m_is_valid = false;
        return ESP_OK;
    }
    return err;
}

esp_err_t WiFiConfigStorage::ensure_config_fallback()
{
    wifi_config_t current_conf;
    esp_err_t err = m_hal.get_config(&current_conf);
    if (err != ESP_OK) {
        return err;
    }

    if (strlen((char *)current_conf.sta.ssid) == 0) {
        if (strlen(CONFIG_WIFI_SSID) > 0) {
            ESP_LOGI(TAG, "No SSID in driver, using Kconfig default: %s", CONFIG_WIFI_SSID);
            wifi_config_t wifi_config = {};
            size_t ssid_len           = strlen(CONFIG_WIFI_SSID);
            if (ssid_len > 32)
                ssid_len = 32;
            memcpy(wifi_config.sta.ssid, CONFIG_WIFI_SSID, ssid_len);

            size_t pass_len = strlen(CONFIG_WIFI_PASSWORD);
            if (pass_len > 64)
                pass_len = 64;
            memcpy(wifi_config.sta.password, CONFIG_WIFI_PASSWORD, pass_len);

            wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
            wifi_config.sta.failure_retry_cnt  = 0;
            wifi_config.sta.pmf_cfg.capable    = true;
            wifi_config.sta.pmf_cfg.required   = false;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            err = m_hal.set_config(&wifi_config);
            if (err == ESP_OK) {
                return save_valid_flag(true);
            }
            return err;
        }
    }
    else {
        // If driver has SSID but flag wasn't set, respect driver
        if (!m_is_valid) {
            return save_valid_flag(true);
        }
    }
    return ESP_OK;
}
