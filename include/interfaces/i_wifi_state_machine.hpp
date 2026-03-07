#pragma once

#include <cstdint>

#include "wifi_types.hpp"

/**
 * @file i_wifi_state_machine.hpp
 * @brief Interface for the Finite State Machine logic of WiFiManager.
 */

namespace wifi_manager {

/**
 * @class IWiFiStateMachine
 * @brief Interface for the Finite State Machine logic of WiFiManager.
 * @internal
 */
class IWiFiStateMachine
{
public:
    using State = wifi_manager::State;
    using CommandId = wifi_manager::CommandId;
    using EventId = wifi_manager::EventId;

    /**
     * @brief Result of an event resolution.
     * @internal
     */
    struct EventOutcome
    {
        State next_state;     ///< The state to transition to
        uint32_t bits_to_set; ///< Bits to set in the sync manager
    };

    /**
     * @brief Properties associated with a state.
     * @internal
     */
    struct StateProps
    {
        bool is_active;    ///< True if WiFi driver is active
        bool is_connected; ///< True if connected and got IP
        bool is_sta_ready; ///< True if WiFi is in STA mode and started
    };

    /**
     * @brief Valid actions for a command.
     * @internal
     */
    enum class Action : uint8_t
    {
        EXECUTE, ///< Execute the command
        SKIP,    ///< Command already executed or redundant
        ERROR    ///< Command not allowed in current state
    };

    virtual ~IWiFiStateMachine() = default;

    /**
     * @brief Validate if a command is allowed in the current state.
     * @internal
     * @param cmd Command ID to validate.
     * @return Action to take.
     */
    virtual Action validate_command(CommandId cmd) const = 0;

    /**
     * @brief Resolve the outcome of an event based on the current state.
     * @internal
     * @param event Event ID to resolve.
     * @return The outcome containing the next state and bits to set.
     */
    virtual EventOutcome resolve_event(EventId event) const = 0;

    /**
     * @brief Transition to a new state.
     * @internal
     * @param next_state The state to transition to.
     */
    virtual void transition_to(State next_state) = 0;

    /**
     * @brief Reset the connection retry counter.
     * @internal
     */
    virtual void reset_retries() = 0;

    /**
     * @brief Handle a suspect connection failure based on RSSI.
     * @internal
     * @param rssi RSSI level at the time of failure.
     * @return true if the failure is considered a final failure (e.g. auth fail).
     */
    virtual bool handle_suspect_failure(int8_t rssi) = 0;

    /**
     * @brief Calculate the delay for the next reconnection attempt.
     * @internal
     * @param[out] delay_ms_out Calculated delay in milliseconds.
     */
    virtual void calculate_next_backoff(uint32_t &delay_ms_out) = 0;

    /**
     * @brief Get the current state of the FSM.
     * @internal
     * @return Current State.
     */
    virtual State get_current_state() const = 0;

    /**
     * @brief Get the current retry count.
     * @internal
     * @return Number of retries.
     */
    virtual uint32_t get_retry_count() const = 0;

    /**
     * @brief Get the system uptime in ms for the next reconnection attempt.
     * @internal
     * @return Uptime in ms.
     */
    virtual uint64_t get_next_reconnect_ms() const = 0;

    /**
     * @brief Get the wait time for the next event or idle tick.
     * @internal
     * @return Wait time in ms.
     */
    virtual uint32_t get_wait_ms() const = 0;

    /**
     * @brief Check if WiFi Station is ready (started).
     * @internal
     * @return true if ready.
     */
    virtual bool is_sta_ready() const = 0;

    /**
     * @brief Check if the WiFi driver is active (started or connected).
     * @internal
     * @return true if active.
     */
    virtual bool is_active() const = 0;
};

} // namespace wifi_manager
