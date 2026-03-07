#pragma once

#include "esp_timer.h"
#include "interfaces/i_timer_hal.hpp"

/**
 * @file esp_timer_hal.hpp
 * @brief ESP-IDF implementation of system time services.
 */

namespace wifi_manager {

/**
 * @class EspTimerHAL
 * @brief ESP-IDF implementation of ITimerHAL using esp_timer.
 */
class EspTimerHAL : public ITimerHAL
{
public:
    EspTimerHAL() = default;
    ~EspTimerHAL() override = default;

    /**
     * @copydoc ITimerHAL::get_time_ms()
     */
    uint64_t get_time_ms() const override
    {
        return esp_timer_get_time() / 1000;
    }
};

} // namespace wifi_manager
