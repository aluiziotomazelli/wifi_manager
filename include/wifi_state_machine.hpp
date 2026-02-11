#pragma once

#include "esp_err.h"
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "wifi_types.hpp"

/**
 * @class WiFiStateMachine
 * @brief Encapsulates the Finite State Machine (FSM) logic for WiFiManager.
 */
class WiFiStateMachine
{
public:
    using State     = wifi_manager::State;
    using CommandId = wifi_manager::CommandId;
    using EventId   = wifi_manager::EventId;

    enum class Action : uint8_t
    {
        EXECUTE,
        SKIP,
        ERROR
    };

    struct EventOutcome
    {
        State next_state;
        EventBits_t bits_to_set;
    };

    struct StateProps
    {
        bool is_active;
        bool is_connected;
        bool is_sta_ready;
    };

    WiFiStateMachine();

    /**
     * @brief Validates if a command can be executed in the current state.
     */
    Action validate_command(CommandId cmd) const;

    /**
     * @brief Resolves the next state and sync bits for an event.
     */
    EventOutcome resolve_event(EventId event) const;

    /**
     * @brief Performs the state transition.
     */
    void transition_to(State next_state);

    /**
     * @brief Resets retry counters.
     */
    void reset_retries();

    /**
     * @brief Handles a suspect failure (potential wrong password or bad signal).
     * @param rssi The RSSI level at the time of disconnection.
     * @return true if too many suspect failures (transits to ERROR_CREDENTIALS).
     */
    bool handle_suspect_failure(int8_t rssi);

    /**
     * @brief Calculates and sets the next reconnection time.
     * @param delay_ms_out [out] The delay calculated.
     */
    void calculate_next_backoff(uint32_t &delay_ms_out);

    // Getters
    State get_current_state() const
    {
        return m_current_state;
    }
    uint32_t get_retry_count() const
    {
        return m_retry_count;
    }
    uint64_t get_next_reconnect_ms() const
    {
        return m_next_reconnect_ms;
    }

    /**
     * @brief Calculate the wait time in FreeRTOS ticks for the task loop.
     * @return portMAX_DELAY if not waiting for reconnect, 0 if reconnect time has passed,
     *         or the calculated ticks to wait until the next reconnect attempt.
     */
    TickType_t get_wait_ticks() const;
    bool is_sta_ready() const;
    bool is_active() const;

    // RSSI thresholds (dBm):
    // GOOD   (-55):  Strong signal, likely credential issue
    // MEDIUM (-67):  Moderate signal, ambiguous failure cause
    // WEAK   (-80):  Weak signal, likely connectivity issue
    // < -80: Critical, always assume signal problem
    static constexpr int8_t RSSI_THRESHOLD_GOOD   = -55;
    static constexpr int8_t RSSI_THRESHOLD_MEDIUM = -67;
    static constexpr int8_t RSSI_THRESHOLD_WEAK   = -80;

    // Retry limits based on signal quality
    static constexpr uint32_t RETRY_LIMIT_GOOD   = 1;
    static constexpr uint32_t RETRY_LIMIT_MEDIUM = 2;
    static constexpr uint32_t RETRY_LIMIT_WEAK   = 5;

    // Backoff parameters
    static constexpr uint32_t MAX_BACKOFF_EXPONENT = 8;
    static constexpr uint32_t MAX_BACKOFF_MS       = 300000UL; // 5 minutes

private:
    State m_current_state;
    uint32_t m_retry_count;
    uint32_t m_suspect_retry_count;
    uint64_t m_next_reconnect_ms;

    static const StateProps s_state_props[(int)State::COUNT];
    static const Action s_command_matrix[(int)State::COUNT][(int)CommandId::COUNT];
    static const EventOutcome s_transition_matrix[(int)State::COUNT][(int)EventId::COUNT];
};
