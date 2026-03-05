// host_test/common/mock_timer_hal.hpp
#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "interfaces/i_timer_hal.hpp"

using namespace wifi_manager;

class MockTimerHAL : public ITimerHAL
{
public:
    MOCK_METHOD(uint64_t, get_time_ms, (), (const, override));
};