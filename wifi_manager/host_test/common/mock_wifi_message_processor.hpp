#pragma once

#include "gmock/gmock.h"
#include "interfaces/i_wifi_message_processor.hpp"

namespace wifi_manager {

class MockWiFiMessageProcessor : public IWiFiMessageProcessor
{
public:
    MOCK_METHOD(void, process_message, (const Message &msg, State state), (override));
    MOCK_METHOD(void, on_idle_tick, (State state), (override));
};

} // namespace wifi_manager
