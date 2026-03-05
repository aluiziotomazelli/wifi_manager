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
    MOCK_METHOD(
        BaseType_t,
        task_create,
        (TaskFunction_t pvTaskCode,
         const char *const pcName,
         const uint32_t usStackDepth,
         void *const pvParameters,
         UBaseType_t uxPriority,
         TaskHandle_t *const pxCreatedTask),
        (override));
    MOCK_METHOD(void, task_delete, (TaskHandle_t xTaskToDelete), (override));
};
