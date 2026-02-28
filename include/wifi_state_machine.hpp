#pragma once

#include "esp_err.h"
#include <cstdint>

#include "wifi_types.hpp"
#include "interfaces/i_timer_hal.hpp"

#include "interfaces/i_wifi_state_machine.hpp"

namespace wifi_manager {

/**
 * @class WiFiStateMachine
 * @brief Encapsulates the Finite State Machine (FSM) logic for WiFiManager.
 */
class WiFiStateMachine : public IWiFiStateMachine
{
public:
    explicit WiFiStateMachine(ITimerHAL &timer_hal);
    ~WiFiStateMachine() override = default;

    /**
     * @brief Validates if a command can be executed in the current state.
     */
    Action validate_command(CommandId cmd) const override;

    /**
     * @brief Resolves the next state and sync bits for an event.
     */
    EventOutcome resolve_event(EventId event) const override;

    /**
     * @brief Performs the state transition.
     */
    void transition_to(State next_state) override;

    /**
     * @brief Resets retry counters.
     */
    void reset_retries() override;

    /**
     * @brief Handles a suspect failure (potential wrong password or bad signal).
     * @param rssi The RSSI level at the time of disconnection.
     * @return true if too many suspect failures (transits to ERROR_CREDENTIALS).
     */
    bool handle_suspect_failure(int8_t rssi) override;

    /**
     * @brief Calculates and sets the next reconnection time.
     * @param delay_ms_out [out] The delay calculated.
     */
    void calculate_next_backoff(uint32_t &delay_ms_out) override;

    // Getters
    State get_current_state() const override
    {
        return current_state_;
    }
    uint32_t get_retry_count() const override
    {
        return retry_count_;
    }
    uint64_t get_next_reconnect_ms() const override
    {
        return next_reconnect_ms_;
    }

    /**
     * @brief Calculate the wait time in milliseconds for the task loop.
     */
    uint32_t get_wait_ms() const override;
    bool is_sta_ready() const override;
    bool is_active() const override;

    // RSSI thresholds (dBm) - Keep static as they are constants
    static constexpr int8_t RSSI_THRESHOLD_GOOD = -55;
    static constexpr int8_t RSSI_THRESHOLD_MEDIUM = -67;
    static constexpr int8_t RSSI_THRESHOLD_WEAK = -80;

    static constexpr uint32_t RETRY_LIMIT_GOOD = 1;
    static constexpr uint32_t RETRY_LIMIT_MEDIUM = 2;
    static constexpr uint32_t RETRY_LIMIT_WEAK = 5;

    static constexpr uint32_t MAX_BACKOFF_EXPONENT = 9;
    static constexpr uint32_t MAX_BACKOFF_MS = 300000UL; // 5 minutes

private:
    ITimerHAL &timer_hal_;
    State current_state_;
    uint32_t retry_count_;
    uint32_t suspect_retry_count_;
    uint64_t next_reconnect_ms_;

    static const StateProps s_state_props[(int)State::COUNT];
    static const Action s_command_matrix[(int)State::COUNT][(int)CommandId::COUNT];
    static const EventOutcome s_transition_matrix[(int)State::COUNT][(int)EventId::COUNT];
};

} // namespace wifi_manager
