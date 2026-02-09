#include "wifi_state_machine.hpp"
#include "esp_timer.h"
#include <algorithm>

// Re-defining bits here or mapping them? Let's use the same values for consistency.
static constexpr EventBits_t STARTED_BIT        = (1 << 0);
static constexpr EventBits_t STOPPED_BIT        = (1 << 1);
static constexpr EventBits_t CONNECTED_BIT      = (1 << 2);
static constexpr EventBits_t DISCONNECTED_BIT   = (1 << 3);
static constexpr EventBits_t CONNECT_FAILED_BIT = (1 << 4);
static constexpr EventBits_t START_FAILED_BIT   = (1 << 5);
static constexpr EventBits_t STOP_FAILED_BIT    = (1 << 6);
static constexpr EventBits_t INVALID_STATE_BIT  = (1 << 7);

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
    : m_current_state(State::UNINITIALIZED)
    , m_retry_count(0)
    , m_suspect_retry_count(0)
    , m_next_reconnect_ms(0)
{
}

WiFiStateMachine::Action WiFiStateMachine::validate_command(CommandId cmd) const
{
    if ((int)cmd >= (int)CommandId::COUNT)
        return Action::EXECUTE;
    return s_command_matrix[(int)m_current_state][(int)cmd];
}

WiFiStateMachine::EventOutcome WiFiStateMachine::resolve_event(EventId event) const
{
    if ((int)event >= (int)EventId::COUNT)
        return {m_current_state, 0};
    return s_transition_matrix[(int)m_current_state][(int)event];
}

void WiFiStateMachine::transition_to(State next_state)
{
    m_current_state = next_state;
}

void WiFiStateMachine::reset_retries()
{
    m_retry_count         = 0;
    m_suspect_retry_count = 0;
}

bool WiFiStateMachine::handle_suspect_failure()
{
    m_suspect_retry_count++;
    if (m_suspect_retry_count >= 3) {
        m_current_state = State::ERROR_CREDENTIALS;
        return true;
    }
    return false;
}

void WiFiStateMachine::calculate_next_backoff(uint32_t &delay_ms_out)
{
    m_retry_count++;

    // Limit exponent to avoid overflow (2^8 = 256)
    uint32_t exponent = (m_retry_count > 0) ? (m_retry_count - 1) : 0;
    if (exponent > 8)
        exponent = 8;

    uint32_t delay_ms = (1UL << exponent) * 1000UL;
    if (delay_ms > 300000UL)
        delay_ms = 300000UL; // Cap at 5 minutes

    delay_ms_out        = delay_ms;
    m_next_reconnect_ms = (esp_timer_get_time() / 1000) + delay_ms;
    m_current_state     = State::WAITING_RECONNECT;
}

bool WiFiStateMachine::is_sta_ready() const
{
    return s_state_props[(int)m_current_state].is_sta_ready;
}

bool WiFiStateMachine::is_active() const
{
    return s_state_props[(int)m_current_state].is_active;
}

TickType_t WiFiStateMachine::get_wait_ticks() const
{
    // Only calculate wait time if we're in the WAITING_RECONNECT state
    if (m_current_state != State::WAITING_RECONNECT) {
        return portMAX_DELAY;
    }

    // Get current time in milliseconds
    uint64_t now_ms = esp_timer_get_time() / 1000;

    // If the reconnect time hasn't arrived yet, calculate how long to wait
    if (m_next_reconnect_ms > now_ms) {
        uint64_t wait_ms = m_next_reconnect_ms - now_ms;
        return pdMS_TO_TICKS(wait_ms);
    }

    // Reconnect time has passed, don't wait
    return 0;
}
