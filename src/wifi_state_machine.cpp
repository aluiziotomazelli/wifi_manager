#include "wifi_state_machine.hpp"
#include "esp_timer.h"
#include <algorithm>

namespace wifi_manager {

const WiFiStateMachine::StateProps WiFiStateMachine::s_state_props[(int)State::COUNT] = {
    /* UNINITIALIZED     */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
    /* INITIALIZING      */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
    /* INITIALIZED       */ {.is_active = false, .is_connected = false, .is_sta_ready = false},
    /* STARTING          */ {.is_active = true, .is_connected = false, .is_sta_ready = false},
    /* STARTED           */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
    /* CONNECTING        */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
    /* CONNECTED_NO_IP   */ {.is_active = true, .is_connected = true, .is_sta_ready = true},
    /* CONNECTED_GOT_IP  */ {.is_active = true, .is_connected = true, .is_sta_ready = true},
    /* DISCONNECTING     */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
    /* WAITING_RECONNECT */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
    /* ERROR_CREDENTIALS */ {.is_active = true, .is_connected = false, .is_sta_ready = true},
    /* STOPPING          */ {.is_active = true, .is_connected = false, .is_sta_ready = false},
};

const WiFiStateMachine::Action WiFiStateMachine::s_command_matrix[(int)State::COUNT][(int)CommandId::COUNT] = {
    // {START,      STOP,          CONNECT,       DISCONNECT,    EXIT}
    {Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR},      // UNINITIALIZED
    {Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR, Action::ERROR},      // INITIALIZING
    {Action::EXECUTE, Action::SKIP, Action::ERROR, Action::ERROR, Action::ERROR},     // INITIALIZED
    {Action::SKIP, Action::EXECUTE, Action::ERROR, Action::ERROR, Action::ERROR},     // STARTING
    {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::SKIP, Action::ERROR},    // STARTED
    {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTING
    {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTED_NO_IP
    {Action::SKIP, Action::EXECUTE, Action::SKIP, Action::EXECUTE, Action::ERROR},    // CONNECTED_GOT_IP
    {Action::SKIP, Action::EXECUTE, Action::ERROR, Action::SKIP, Action::ERROR},      // DISCONNECTING
    {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::EXECUTE, Action::ERROR}, // WAITING_RECONNECT
    {Action::SKIP, Action::EXECUTE, Action::EXECUTE, Action::EXECUTE, Action::ERROR}, // ERROR_CREDENTIALS
    {Action::ERROR, Action::SKIP, Action::ERROR, Action::ERROR, Action::ERROR},       // STOPPING
};

const WiFiStateMachine::EventOutcome WiFiStateMachine::s_transition_matrix[(int)State::COUNT][(int)EventId::COUNT] = {
    /* UNINITIALIZED  */
    {{State::UNINITIALIZED, 0},
     {State::UNINITIALIZED, 0},
     {State::UNINITIALIZED, 0},
     {State::UNINITIALIZED, 0},
     {State::UNINITIALIZED, 0},
     {State::UNINITIALIZED, 0}},
    /* INITIALIZING   */
    {{State::INITIALIZING, 0},
     {State::INITIALIZING, 0},
     {State::INITIALIZING, 0},
     {State::INITIALIZING, 0},
     {State::INITIALIZING, 0},
     {State::INITIALIZING, 0}},
    /* INITIALIZED    */
    {{State::INITIALIZED, 0},
     {State::INITIALIZED, 0},
     {State::INITIALIZED, 0},
     {State::INITIALIZED, 0},
     {State::INITIALIZED, 0},
     {State::INITIALIZED, 0}},
    /* STARTING       */
    {{State::STARTED, STARTED_BIT},
     {State::STARTING, 0},
     {State::STARTING, 0},
     {State::INITIALIZED, START_FAILED_BIT},
     {State::STARTING, 0},
     {State::STARTING, 0}},
    /* STARTED        */
    {{State::STARTED, 0},
     {State::STARTED, 0},
     {State::STARTED, 0},
     {State::STARTED, 0},
     {State::STARTED, 0},
     {State::STARTED, 0}},
    /* CONNECTING     */
    {{State::CONNECTING, 0},
     {State::CONNECTING, 0},
     {State::CONNECTED_NO_IP, 0},
     {State::WAITING_RECONNECT, 0},
     {State::CONNECTED_GOT_IP, CONNECTED_BIT},
     {State::CONNECTING, 0}},
    /* CONNECTED_NO_IP*/
    {{State::CONNECTED_NO_IP, 0},
     {State::CONNECTED_NO_IP, 0},
     {State::CONNECTED_NO_IP, 0},
     {State::WAITING_RECONNECT, 0},
     {State::CONNECTED_GOT_IP, CONNECTED_BIT},
     {State::CONNECTED_NO_IP, 0}},
    /* CONNECTED_GOT_IP*/
    {{State::CONNECTED_GOT_IP, 0},
     {State::CONNECTED_GOT_IP, 0},
     {State::CONNECTED_GOT_IP, 0},
     {State::WAITING_RECONNECT, 0},
     {State::CONNECTED_GOT_IP, 0},
     {State::CONNECTED_NO_IP, 0}},
    /* DISCONNECTING  */
    {{State::DISCONNECTING, 0},
     {State::DISCONNECTING, 0},
     {State::DISCONNECTING, 0},
     {State::STARTED, DISCONNECTED_BIT},
     {State::DISCONNECTING, 0},
     {State::DISCONNECTING, 0}},
    /* WAITING_RECON  */
    {{State::WAITING_RECONNECT, 0},
     {State::WAITING_RECONNECT, 0},
     {State::WAITING_RECONNECT, 0},
     {State::WAITING_RECONNECT, 0},
     {State::WAITING_RECONNECT, 0},
     {State::WAITING_RECONNECT, 0}},
    /* ERROR_CRED     */
    {{State::ERROR_CREDENTIALS, 0},
     {State::ERROR_CREDENTIALS, 0},
     {State::ERROR_CREDENTIALS, 0},
     {State::ERROR_CREDENTIALS, 0},
     {State::ERROR_CREDENTIALS, 0},
     {State::ERROR_CREDENTIALS, 0}},
    /* STOPPING       */
    {{State::STOPPING, 0},
     {State::INITIALIZED, STOPPED_BIT},
     {State::STOPPING, 0},
     {State::STOPPING, 0},
     {State::STOPPING, 0},
     {State::STOPPING, 0}},
};

WiFiStateMachine::WiFiStateMachine()
    : current_state_(State::UNINITIALIZED)
    , retry_count_(0)
    , suspect_retry_count_(0)
    , next_reconnect_ms_(0)
{
}

WiFiStateMachine::Action WiFiStateMachine::validate_command(CommandId cmd) const
{
    if ((int)cmd >= (int)CommandId::COUNT)
        return Action::EXECUTE;
    return s_command_matrix[(int)current_state_][(int)cmd];
}

WiFiStateMachine::EventOutcome WiFiStateMachine::resolve_event(EventId event) const
{
    if ((int)event >= (int)EventId::COUNT)
        return {current_state_, 0};
    return s_transition_matrix[(int)current_state_][(int)event];
}

void WiFiStateMachine::transition_to(State next_state)
{
    current_state_ = next_state;
}

void WiFiStateMachine::reset_retries()
{
    retry_count_ = 0;
    suspect_retry_count_ = 0;
}

bool WiFiStateMachine::handle_suspect_failure(int8_t rssi)
{
    suspect_retry_count_++;

    uint32_t limit = 0;

    if (rssi >= RSSI_THRESHOLD_GOOD) {
        limit = RETRY_LIMIT_GOOD;
    }
    else if (rssi >= RSSI_THRESHOLD_MEDIUM) {
        limit = RETRY_LIMIT_MEDIUM;
    }
    else if (rssi >= RSSI_THRESHOLD_WEAK) {
        limit = RETRY_LIMIT_WEAK;
    }
    else {
        return false;
    }

    if (suspect_retry_count_ >= limit) {
        current_state_ = State::ERROR_CREDENTIALS;
        return true;
    }
    return false;
}

void WiFiStateMachine::calculate_next_backoff(uint32_t &delay_ms_out)
{
    retry_count_++;

    uint32_t exponent = (retry_count_ > 0) ? (retry_count_ - 1) : 0;
    if (exponent > MAX_BACKOFF_EXPONENT)
        exponent = MAX_BACKOFF_EXPONENT;

    uint32_t delay_ms = (1UL << exponent) * 1000UL;
    if (delay_ms > MAX_BACKOFF_MS)
        delay_ms = MAX_BACKOFF_MS;

    delay_ms_out = delay_ms;
    next_reconnect_ms_ = (esp_timer_get_time() / 1000) + delay_ms;
    current_state_ = State::WAITING_RECONNECT;
}

bool WiFiStateMachine::is_sta_ready() const
{
    return s_state_props[(int)current_state_].is_sta_ready;
}

bool WiFiStateMachine::is_active() const
{
    return s_state_props[(int)current_state_].is_active;
}

TickType_t WiFiStateMachine::get_wait_ticks() const
{
    if (current_state_ != State::WAITING_RECONNECT) {
        return portMAX_DELAY;
    }

    uint64_t now_ms = esp_timer_get_time() / 1000;

    if (next_reconnect_ms_ > now_ms) {
        uint64_t wait_ms = next_reconnect_ms_ - now_ms;
        if (wait_ms > UINT32_MAX / portTICK_PERIOD_MS) {
            return portMAX_DELAY;
        }
        return pdMS_TO_TICKS(wait_ms);
    }

    return 0;
}

} // namespace wifi_manager
