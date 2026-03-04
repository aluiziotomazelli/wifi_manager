// host_test/common/mock_wifi_driver_hal.hpp
#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "interfaces/i_wifi_driver_hal.hpp"

using namespace wifi_manager;

class MockWiFiDriverHAL : public IWiFiDriverHAL
{
public:
    MOCK_METHOD(esp_err_t, netif_init, (), (override));
    MOCK_METHOD(esp_err_t, event_loop_create_default, (), (override));
    MOCK_METHOD(esp_netif_t *, netif_create_default_wifi_sta, (), (override));
    MOCK_METHOD(esp_netif_t *, netif_get_handle_from_ifkey, (const char *if_key), (override));
    MOCK_METHOD(esp_err_t, wifi_init, (wifi_init_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_set_mode, (wifi_mode_t mode), (override));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_register,
        (esp_event_base_t event_base,
         int32_t event_id,
         esp_event_handler_t event_handler,
         void *handler_arg,
         esp_event_handler_instance_t *instance),
        (override));
    MOCK_METHOD(
        esp_err_t,
        event_handler_instance_unregister,
        (esp_event_base_t event_base, int32_t event_id, esp_event_handler_instance_t instance),
        (override));
    MOCK_METHOD(esp_err_t, wifi_start, (), (override));
    MOCK_METHOD(esp_err_t, wifi_stop, (), (override));
    MOCK_METHOD(esp_err_t, wifi_connect, (), (override));
    MOCK_METHOD(esp_err_t, wifi_disconnect, (), (override));
    MOCK_METHOD(esp_err_t, wifi_restore, (), (override));
    MOCK_METHOD(esp_err_t, wifi_set_config, (wifi_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_get_config, (wifi_config_t * cfg), (override));
    MOCK_METHOD(esp_err_t, wifi_deinit, (), (override));
    MOCK_METHOD(void, netif_destroy_default_wifi, (esp_netif_t * netif), (override));
};

class MockWiFiSyncManager : public IWiFiSyncManager
{
public:
    MOCK_METHOD(esp_err_t, init, (), (override));
    MOCK_METHOD(void, deinit, (), (override));
    MOCK_METHOD(esp_err_t, post_message, (const wifi_manager::Message &msg), (override));
    MOCK_METHOD(esp_err_t, post_message_from_isr, (const wifi_manager::Message &msg), (override));
    MOCK_METHOD(void, clear_bits, (uint32_t), (override));
    MOCK_METHOD(void, set_bits, (uint32_t), (override));
    MOCK_METHOD(uint32_t, wait_for_bits, (uint32_t, uint32_t), (override));
    MOCK_METHOD(bool, is_initialized, (), (const, override));
    MOCK_METHOD(QueueHandle_t, get_queue, (), (const, override));
    MOCK_METHOD(EventGroupHandle_t, get_event_group, (), (const, override));
};