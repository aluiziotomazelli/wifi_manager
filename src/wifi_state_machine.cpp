#include "wifi_state_machine.hpp"
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
    {{State::UNINITIALIZED, 0},  ///< STA_START
     {State::UNINITIALIZED, 0},  ///< STA_STOP
     {State::UNINITIALIZED, 0},  ///< STA_CONNECT
     {State::UNINITIALIZED, 0},  ///< STA_DISCONNECT
     {State::UNINITIALIZED, 0},  ///< GOT_IP
     {State::UNINITIALIZED, 0}}, ///< LOST_IP
    /* INITIALIZING   */
    {{State::INITIALIZING, 0},  ///< STA_START
     {State::INITIALIZING, 0},  ///< STA_STOP
     {State::INITIALIZING, 0},  ///< STA_CONNECT
     {State::INITIALIZING, 0},  ///< STA_DISCONNECT
     {State::INITIALIZING, 0},  ///< GOT_IP
     {State::INITIALIZING, 0}}, ///< LOST_IP
    /* INITIALIZED    */
    {{State::INITIALIZED, 0},  ///< STA_START
     {State::INITIALIZED, 0},  ///< STA_STOP
     {State::INITIALIZED, 0},  ///< STA_CONNECT
     {State::INITIALIZED, 0},  ///< STA_DISCONNECT
     {State::INITIALIZED, 0},  ///< GOT_IP
     {State::INITIALIZED, 0}}, ///< LOST_IP
    /* STARTING       */
    {{State::STARTED, STARTED_BIT},          ///< STA_START
     {State::STARTING, 0},                   ///< STA_STOP
     {State::STARTING, 0},                   ///< STA_CONNECT
     {State::INITIALIZED, START_FAILED_BIT}, ///< STA_DISCONNECT
     {State::STARTING, 0},                   ///< GOT_IP
     {State::STARTING, 0}},                  ///< LOST_IP
    /* STARTED        */
    {{State::STARTED, 0},  ///< STA_START
     {State::STARTED, 0},  ///< STA_STOP
     {State::STARTED, 0},  ///< STA_CONNECT
     {State::STARTED, 0},  ///< STA_DISCONNECT
     {State::STARTED, 0},  ///< GOT_IP
     {State::STARTED, 0}}, ///< LOST_IP
    /* CONNECTING     */
    {{State::CONNECTING, 0},                   ///< STA_START
     {State::CONNECTING, 0},                   ///< STA_STOP
     {State::CONNECTED_NO_IP, 0},              ///< STA_CONNECT
     {State::WAITING_RECONNECT, 0},            ///< STA_DISCONNECT
     {State::CONNECTED_GOT_IP, CONNECTED_BIT}, ///< GOT_IP
     {State::CONNECTING, 0}},                  ///< LOST_IP
    /* CONNECTED_NO_IP*/
    {{State::CONNECTED_NO_IP, 0},              //< STA_START
     {State::CONNECTED_NO_IP, 0},              ///< STA_STOP
     {State::CONNECTED_NO_IP, 0},              ///< STA_CONNECTED
     {State::WAITING_RECONNECT, 0},            ///< STA_DISCONNECTED
     {State::CONNECTED_GOT_IP, CONNECTED_BIT}, ///< GOT_IP
     {State::CONNECTED_NO_IP, 0}},             ///< LOST_IP
    /* CONNECTED_GOT_IP*/
    {{State::CONNECTED_GOT_IP, 0},  //< STA_START
     {State::CONNECTED_GOT_IP, 0},  //< STA_STOP
     {State::CONNECTED_GOT_IP, 0},  //< STA_CONNECTED
     {State::WAITING_RECONNECT, 0}, //< STA_DISCONNECTED
     {State::CONNECTED_GOT_IP, 0},  //< GOT_IP
     {State::CONNECTED_NO_IP, 0}},  //< LOST_IP
    /* DISCONNECTING  */
    {{State::DISCONNECTING, 0},          ///< STA_START
     {State::DISCONNECTING, 0},          ///< STA_STOP
     {State::DISCONNECTING, 0},          ///< STA_CONNECTED
     {State::STARTED, DISCONNECTED_BIT}, ///< STA_DISCONNECTED
     {State::DISCONNECTING, 0},          ///< GOT_IP
     {State::DISCONNECTING, 0}},         ///< LOST_IP
    /* WAITING_RECON  */
    {{State::WAITING_RECONNECT, 0},  ///< STA_START
     {State::WAITING_RECONNECT, 0},  ///< STA_STOP
     {State::WAITING_RECONNECT, 0},  ///< STA_CONNECTED
     {State::WAITING_RECONNECT, 0},  ///< STA_DISCONNECTED
     {State::WAITING_RECONNECT, 0},  ///< GOT_IP
     {State::WAITING_RECONNECT, 0}}, ///< LOST_IP
    /* ERROR_CRED     */
    {{State::ERROR_CREDENTIALS, 0},  ///< STA_START
     {State::ERROR_CREDENTIALS, 0},  ///< STA_STOP
     {State::ERROR_CREDENTIALS, 0},  ///< STA_CONNECTED
     {State::ERROR_CREDENTIALS, 0},  ///< STA_DISCONNECTED
     {State::ERROR_CREDENTIALS, 0},  ///< GOT_IP
     {State::ERROR_CREDENTIALS, 0}}, ///< LOST_IP
    /* STOPPING       */
    {{State::STOPPING, 0},              ///< STA_START
     {State::INITIALIZED, STOPPED_BIT}, ///< STA_STOP
     {State::STOPPING, 0},              ///< STA_CONNECTED
     {State::STOPPING, 0},              ///< STA_DISCONNECTED
     {State::STOPPING, 0},              ///< GOT_IP
     {State::STOPPING, 0}},             ///< LOST_IP
};

WiFiStateMachine::WiFiStateMachine(ITimerHAL &timer_hal)
    : timer_hal_(timer_hal)
    , current_state_(State::UNINITIALIZED)
    , retry_count_(0)
    , suspect_retry_count_(0)
    , next_reconnect_ms_(0)
{
}

WiFiStateMachine::Action WiFiStateMachine::validate_command(CommandId cmd) const
{
    if ((int)cmd >= (int)CommandId::COUNT)
        return Action::EXECUTE; // TODO: change to ERROR?
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

    uint32_t exponent = retry_count_ - 1;

    if (exponent > MAX_BACKOFF_EXPONENT)
        exponent = MAX_BACKOFF_EXPONENT;

    uint32_t delay_ms = (1UL << exponent) * 1000UL;
    if (delay_ms > MAX_BACKOFF_MS)
        delay_ms = MAX_BACKOFF_MS;

    delay_ms_out = delay_ms;
    next_reconnect_ms_ = timer_hal_.get_time_ms() + delay_ms;
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

uint32_t WiFiStateMachine::get_wait_ms() const
{
    if (current_state_ != State::WAITING_RECONNECT) {
        return 0xFFFFFFFF; // Equivalent to portMAX_DELAY
    }

    uint64_t now_ms = timer_hal_.get_time_ms();

    if (next_reconnect_ms_ > now_ms) {
        uint64_t wait_ms = next_reconnect_ms_ - now_ms;
        if (wait_ms > 0xFFFFFFFF) {
            return 0xFFFFFFFF;
        }
        return (uint32_t)wait_ms;
    }

    return 0;
}

} // namespace wifi_manager
