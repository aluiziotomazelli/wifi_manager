// host_test/test_config_storage/main/test_config_storage.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"

using namespace wifi_manager;
using namespace testing;

class MockWiFiDriverHAL : public IWiFiDriverHAL
{
public:
    MOCK_METHOD(esp_err_t, netif_init, ());
    MOCK_METHOD(esp_err_t, event_loop_create_default, ());
    MOCK_METHOD(esp_netif_t *, netif_create_default_wifi_sta, ());
    MOCK_METHOD(esp_netif_t *, netif_get_handle_from_ifkey, (const char *if_key));
    MOCK_METHOD(esp_err_t, wifi_init, (wifi_init_config_t * cfg));
    MOCK_METHOD(esp_err_t, wifi_set_mode, (wifi_mode_t mode));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_register,
        (esp_event_base_t event_base,
         int32_t event_id,
         esp_event_handler_t event_handler,
         void *handler_arg,
         esp_event_handler_instance_t *instance));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_unregister,
        (esp_event_base_t event_base, int32_t event_id, esp_event_handler_instance_t instance));
    MOCK_METHOD(esp_err_t, wifi_start, ());
    MOCK_METHOD(esp_err_t, wifi_stop, ());
    MOCK_METHOD(esp_err_t, wifi_connect, ());
    MOCK_METHOD(esp_err_t, wifi_disconnect, ());
    MOCK_METHOD(esp_err_t, wifi_restore, ());
    MOCK_METHOD(esp_err_t, wifi_set_config, (wifi_config_t * cfg));
    MOCK_METHOD(esp_err_t, wifi_get_config, (wifi_config_t * cfg));
    MOCK_METHOD(esp_err_t, wifi_deinit, ());
    MOCK_METHOD(void, netif_destroy_default_wifi, (esp_netif_t * netif));
};

class ConfigStorageTest : public ::testing::Test
{
protected:
    MockWiFiDriverHAL hal;
    WiFiConfigStorage storage{&hal, "test_wifi"};
};

TEST_F(ConfigStorageTest, NvsInit)
{
    nvs_flash_erase();
    EXPECT_EQ(nvs_flash_init(), ESP_OK);
    nvs_flash_deinit();
}