// host_test/common/mock_wifi_state_machine.hpp
#pragma once

#include "gmock/gmock.h"
#include "interfaces/i_wifi_config_storage.hpp"

namespace wifi_manager {

class MockWiFiConfigStorage : public IWiFiConfigStorage
{
public:
    MOCK_METHOD(esp_err_t, init, (), (override));
    MOCK_METHOD(esp_err_t, add_credentials, (const std::string &ssid, const std::string &password), (override));
    MOCK_METHOD(esp_err_t, load_credentials, (std::string & ssid, std::string &password), (override));
    MOCK_METHOD(esp_err_t, clear_credentials, (), (override));
    MOCK_METHOD(esp_err_t, factory_reset, (), (override));
    MOCK_METHOD(bool, is_valid, (), (const, override));
    MOCK_METHOD(esp_err_t, save_valid_flag, (bool valid), (override));
};

} // namespace wifi_manager