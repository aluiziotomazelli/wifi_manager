#pragma once

#include <cstdint>

namespace wifi_manager {

/**
 * @class ITimerHAL
 * @brief Interface for system time services.
 */
class ITimerHAL
{
public:
    virtual ~ITimerHAL() = default;

    /**
     * @brief Get system time in milliseconds.
     * @return uint64_t uptime in ms.
     */
    virtual uint64_t get_time_ms() const = 0;
};

} // namespace wifi_manager
