// host_test/common/mock_wifi_state_machine.hpp
#pragma once

#include "gmock/gmock.h"
#include "interfaces/i_wifi_state_machine.hpp"

namespace wifi_manager {

class MockWiFiStateMachine : public IWiFiStateMachine
{
public:
    MOCK_METHOD(Action, validate_command, (CommandId cmd), (const, override));
    MOCK_METHOD(EventOutcome, resolve_event, (EventId event), (const, override));
    MOCK_METHOD(void, transition_to, (State next_state), (override));
    MOCK_METHOD(void, reset_retries, (), (override));
    MOCK_METHOD(bool, handle_suspect_failure, (int8_t rssi), (override));
    MOCK_METHOD(void, calculate_next_backoff, (uint32_t &delay_ms_out), (override));
    MOCK_METHOD(State, get_current_state, (), (const, override));
    MOCK_METHOD(uint32_t, get_retry_count, (), (const, override));
    MOCK_METHOD(uint64_t, get_next_reconnect_ms, (), (const, override));
    MOCK_METHOD(uint32_t, get_wait_ms, (), (const, override));
    MOCK_METHOD(bool, is_sta_ready, (), (const, override));
    MOCK_METHOD(bool, is_active, (), (const, override));
};

} // namespace wifi_manager