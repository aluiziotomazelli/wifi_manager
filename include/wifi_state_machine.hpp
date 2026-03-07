#pragma once

#include "esp_err.h"
#include <cstdint>

#include "wifi_types.hpp"
#include "interfaces/i_timer_hal.hpp"

#include "interfaces/i_wifi_state_machine.hpp"

/**
 * @file wifi_state_machine.hpp
 * @brief Concrete implementation of IWiFiStateMachine.
 */

namespace wifi_manager {

/**
 * @class WiFiStateMachine
 * @brief Encapsulates the Finite State Machine (FSM) logic for WiFiManager.
 */
class WiFiStateMachine : public IWiFiStateMachine
{
public:
    /**
     * @brief Construct a new WiFiStateMachine.
     *
     * @param timer_hal Reference to the timer HAL.
     */
    explicit WiFiStateMachine(ITimerHAL &timer_hal);

    ~WiFiStateMachine() override = default;

    /**
     * @copydoc IWiFiStateMachine::validate_command()
     */
    Action validate_command(CommandId cmd) const override;

    /**
     * @copydoc IWiFiStateMachine::resolve_event()
     */
    EventOutcome resolve_event(EventId event) const override;

    /**
     * @copydoc IWiFiStateMachine::transition_to()
     */
    void transition_to(State next_state) override;

    /**
     * @copydoc IWiFiStateMachine::reset_retries()
     */
    void reset_retries() override;

    /**
     * @copydoc IWiFiStateMachine::handle_suspect_failure()
     */
    bool handle_suspect_failure(int8_t rssi) override;

    /**
     * @copydoc IWiFiStateMachine::calculate_next_backoff()
     */
    void calculate_next_backoff(uint32_t &delay_ms_out) override;

    /**
     * @copydoc IWiFiStateMachine::get_current_state()
     */
    State get_current_state() const override
    {
        return current_state_;
    }

    /**
     * @copydoc IWiFiStateMachine::get_retry_count()
     */
    uint32_t get_retry_count() const override
    {
        return retry_count_;
    }

    /**
     * @copydoc IWiFiStateMachine::get_next_reconnect_ms()
     */
    uint64_t get_next_reconnect_ms() const override
    {
        return next_reconnect_ms_;
    }

    /**
     * @copydoc IWiFiStateMachine::get_wait_ms()
     */
    uint32_t get_wait_ms() const override;

    /**
     * @copydoc IWiFiStateMachine::is_sta_ready()
     */
    bool is_sta_ready() const override;

    /**
     * @copydoc IWiFiStateMachine::is_active()
     */
    bool is_active() const override;

    // RSSI thresholds (dBm)
    static constexpr int8_t RSSI_THRESHOLD_GOOD = -55;   ///< Threshold for good signal
    static constexpr int8_t RSSI_THRESHOLD_MEDIUM = -67; ///< Threshold for medium signal
    static constexpr int8_t RSSI_THRESHOLD_WEAK = -80;   ///< Threshold for weak signal

    static constexpr uint32_t RETRY_LIMIT_GOOD = 1;   ///< Max retries for good signal
    static constexpr uint32_t RETRY_LIMIT_MEDIUM = 2; ///< Max retries for medium signal
    static constexpr uint32_t RETRY_LIMIT_WEAK = 5;   ///< Max retries for weak signal

    static constexpr uint32_t MAX_BACKOFF_EXPONENT = 9;      ///< Maximum exponent for backoff calculation
    static constexpr uint32_t MAX_BACKOFF_MS = 300000UL;     ///< Maximum backoff delay (5 minutes)

private:
    ITimerHAL &timer_hal_;           ///< Reference to the timer HAL
    State current_state_;            ///< Current state of the FSM
    uint32_t retry_count_;           ///< Total connection retry count
    uint32_t suspect_retry_count_;   ///< Consecutive suspect failure count
    uint64_t next_reconnect_ms_;     ///< System uptime in ms for next reconnect

    static const StateProps s_state_props[(int)State::COUNT];                                ///< Properties for each state
    static const Action s_command_matrix[(int)State::COUNT][(int)CommandId::COUNT];           ///< Matrix for command validation
    static const EventOutcome s_transition_matrix[(int)State::COUNT][(int)EventId::COUNT];   ///< Matrix for event resolution
};

} // namespace wifi_manager
