// include/mocks/mock_i_wifi_manager.hpp
#pragma once

#include <gmock/gmock.h>
#include "interfaces/i_wifi_manager.hpp"

namespace wifi_manager {

class MockWiFiManager : public IWiFiManager
{
public:
    MOCK_METHOD(esp_err_t, init, (const Config& config), (override));
    MOCK_METHOD(esp_err_t, deinit, (), (override));
    MOCK_METHOD(esp_err_t, start, (uint32_t timeout_ms), (override));
    MOCK_METHOD(esp_err_t, start, (), (override));
    MOCK_METHOD(esp_err_t, stop, (uint32_t timeout_ms), (override));
    MOCK_METHOD(esp_err_t, stop, (), (override));
    MOCK_METHOD(esp_err_t, connect, (uint32_t timeout_ms, uint8_t max_retries, uint32_t base_delay_ms), (override));
    MOCK_METHOD(esp_err_t, connect, (), (override));
    MOCK_METHOD(esp_err_t, disconnect, (uint32_t timeout_ms), (override));
    MOCK_METHOD(esp_err_t, disconnect, (), (override));
    MOCK_METHOD(State, get_state, (), (const, override));
    MOCK_METHOD(esp_err_t, add_credentials, (const std::string& ssid, const std::string& password), (override));
    MOCK_METHOD(esp_err_t, set_credentials, (const std::string& ssid, const std::string& password), (override));
    MOCK_METHOD(esp_err_t, get_credentials, (std::string & ssid, std::string& password), (override));
    MOCK_METHOD(esp_err_t, clear_credentials, (), (override));
    MOCK_METHOD(esp_err_t, factory_reset, (), (override));
    MOCK_METHOD(bool, is_credentials_valid, (), (const, override));
    MOCK_METHOD(TaskHandle_t, get_task_handle, (), (const, override));
    MOCK_METHOD(esp_err_t, get_ap_info, (wifi_ap_record_t & info), (override));
    MOCK_METHOD(esp_err_t, get_rssi, (int8_t & rssi), (override));
};

} // namespace wifi_manager
