#include "wifi_config_storage.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi_driver_hal.hpp"
#include <cstring>

namespace wifi_manager {

static const char *TAG = "WiFiConfigStorage";

static constexpr uint8_t MAX_AP = 10;

WiFiConfigStorage::WiFiConfigStorage(IWiFiDriverHAL &hal, const char *nvs_namespace)
    : hal_(hal)
    , nvs_namespace_(nvs_namespace)
    , is_valid_(false)
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

    if (err == ESP_OK) {
        err = load_valid_flag();
    }
    return err;
}

esp_err_t WiFiConfigStorage::add_credentials(const std::string &ssid, const std::string &password)
{
    if (ssid.length() > 32 || password.length() > 64) {
        ESP_LOGE(TAG, "SSID or password too long");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    uint8_t count = 0;
    nvs_get_u8(h, "ap_count", &count);

    int8_t found_idx = -1;
    for (int i = 0; i < count; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ap_%d_ssid", i);
        char stored_ssid[33] = {0};
        size_t len = sizeof(stored_ssid);
        if (nvs_get_str(h, key, stored_ssid, &len) == ESP_OK) {
            if (ssid == stored_ssid) {
                found_idx = i;
                break;
            }
        }
    }

    int8_t target_idx = found_idx;
    if (target_idx == -1) {
        if (count < MAX_AP) {
            target_idx = count;
            count++;
            nvs_set_u8(h, "ap_count", count);
        }
        else {
            // Replaces last one if full, or we could implement a circular buffer.
            // For now, let's just use 9.
            target_idx = 9;
        }
    }

    char ssid_key[16], pass_key[16];
    snprintf(ssid_key, sizeof(ssid_key), "ap_%d_ssid", target_idx);
    snprintf(pass_key, sizeof(pass_key), "ap_%d_pass", target_idx);

    nvs_set_str(h, ssid_key, ssid.c_str());
    nvs_set_str(h, pass_key, password.c_str());
    nvs_set_u8(h, "ap_cur_idx", (uint8_t)target_idx);

    err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        err = sync_to_driver(ssid, password);
    }

    if (err == ESP_OK) {
        err = save_valid_flag(true);
    }

    return err;
}

esp_err_t WiFiConfigStorage::load_credentials(std::string &ssid, std::string &password)
{
    ESP_LOGD(TAG, "Loading credentials");
    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    else if (err != ESP_OK) {
        return err;
    }

    uint8_t cur_idx = 0;
    nvs_get_u8(h, "ap_cur_idx", &cur_idx);

    char ssid_key[16], pass_key[16];
    snprintf(ssid_key, sizeof(ssid_key), "ap_%d_ssid", cur_idx);
    snprintf(pass_key, sizeof(pass_key), "ap_%d_pass", cur_idx);

    char ssid_buf[33] = {0};
    size_t ssid_len = sizeof(ssid_buf);
    err = nvs_get_str(h, ssid_key, ssid_buf, &ssid_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_ERR_NOT_FOUND;
    }
    else if (err == ESP_OK) {
        ssid = ssid_buf;
        char pass_buf[65] = {0};
        size_t pass_len = sizeof(pass_buf);
        err = nvs_get_str(h, pass_key, pass_buf, &pass_len);
        if (err == ESP_OK) {
            password = pass_buf;
        }
    }

    nvs_close(h);
    return err;
}

esp_err_t WiFiConfigStorage::clear_credentials()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }

    wifi_config_t saved_config = {};
    err = hal_.wifi_set_config(&saved_config);

    if (err == ESP_OK) {
        is_valid_ = false;
    }
    return err;
}

esp_err_t WiFiConfigStorage::factory_reset()
{
    hal_.wifi_restore();

    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_erase_all(h);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }

    is_valid_ = false;
    return err;
}

bool WiFiConfigStorage::is_valid() const
{
    return is_valid_;
}

esp_err_t WiFiConfigStorage::save_valid_flag(bool valid)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READWRITE, &h);

    if (err == ESP_OK) {
        err = nvs_set_u8(h, "valid", valid ? 1 : 0);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
        if (err == ESP_OK) {
            is_valid_ = valid;
        }
    }

    return err;
}

esp_err_t WiFiConfigStorage::load_valid_flag()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(nvs_namespace_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NVS partition not found, assuming invalid");
        is_valid_ = false;
        return ESP_OK; // first execution, namespace doesn't exist yet
    }
    if (err == ESP_OK) {
        uint8_t valid = 0;
        if (nvs_get_u8(h, "valid", &valid) == ESP_OK) {
            ESP_LOGD(TAG, "NVS partition found, assuming valid");
            is_valid_ = (valid != 0);
        }
        nvs_close(h);
    }
    return err;
}

esp_err_t WiFiConfigStorage::sync_to_driver(const std::string &ssid, const std::string &password)
{
    wifi_config_t wifi_config = {};
    size_t ssid_len = ssid.length();
    memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid_len);

    size_t pass_len = password.length();
    memcpy(wifi_config.sta.password, password.c_str(), pass_len);

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt = 0;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    return hal_.wifi_set_config(&wifi_config);
}

} // namespace wifi_manager
