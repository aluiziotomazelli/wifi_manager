#pragma once

#include <cstdint>

/**
 * @file i_timer_hal.hpp
 * @brief Interface for system time services.
 */

namespace wifi_manager {

/**
 * @class ITimerHAL
 * @brief Interface for system time services.
 * @internal
 */
class ITimerHAL
{
public:
    virtual ~ITimerHAL() = default;

    /**
     * @brief Get system time in milliseconds.
     * @internal
     * @return uint64_t uptime in ms.
     */
    virtual uint64_t get_time_ms() const = 0;
};

} // namespace wifi_manager
