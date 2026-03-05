// host_test/common/mock_wifi_sync_manager.hpp
#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "interfaces/i_wifi_sync_manager.hpp"

using namespace wifi_manager;

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