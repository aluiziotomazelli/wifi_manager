#pragma once

#include <cstdint>

#include "wifi_types.hpp"

namespace wifi_manager {

/**
 * @class IWiFiStateMachine
 * @brief Interface for the Finite State Machine logic of WiFiManager.
 */
class IWiFiStateMachine
{
public:
    using State = wifi_manager::State;
    using CommandId = wifi_manager::CommandId;
    using EventId = wifi_manager::EventId;

    /**
     * @brief Result of an event resolution.
     */
    struct EventOutcome
    {
        State next_state;
        uint32_t bits_to_set;
    };

    /**
     * @brief Properties associated with a state.
     */
    struct StateProps
    {
        bool is_active;
        bool is_connected;
        bool is_sta_ready;
    };

    /**
     * @brief Valid actions for a command.
     */
    enum class Action : uint8_t
    {
        EXECUTE,
        SKIP,
        ERROR
    };

    virtual ~IWiFiStateMachine() = default;

    virtual Action validate_command(CommandId cmd) const = 0;
    virtual EventOutcome resolve_event(EventId event) const = 0;
    virtual void transition_to(State next_state) = 0;
    virtual void reset_retries() = 0;
    virtual bool handle_suspect_failure(int8_t rssi) = 0;
    virtual void calculate_next_backoff(uint32_t &delay_ms_out) = 0;

    virtual State get_current_state() const = 0;
    virtual uint32_t get_retry_count() const = 0;
    virtual uint64_t get_next_reconnect_ms() const = 0;

    virtual uint32_t get_wait_ms() const = 0;
    virtual bool is_sta_ready() const = 0;
    virtual bool is_active() const = 0;
};

} // namespace wifi_manager
