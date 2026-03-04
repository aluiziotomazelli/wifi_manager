// host_test/common/mock_wifi_event_handler.hpp
#pragma once

#include "gmock/gmock.h"
#include "interfaces/i_wifi_event_handler.hpp"

namespace wifi_manager {

class MockWiFiEventHandler : public IWiFiEventHandler
{
public:
    MOCK_METHOD(void, handle_wifi_event, (esp_event_base_t base, int32_t id, void *data), (override));
    MOCK_METHOD(void, handle_ip_event, (esp_event_base_t base, int32_t id, void *data), (override));
};

} // namespace wifi_manager