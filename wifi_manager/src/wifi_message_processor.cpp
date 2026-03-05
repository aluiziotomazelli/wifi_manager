#include "wifi_message_processor.hpp"
#include "esp_log.h"
#include "esp_wifi.h"

namespace wifi_manager {

WiFiMessageProcessor::WiFiMessageProcessor(
    IWiFiDriverHAL &driver_hal,
    IWiFiConfigStorage &storage,
    IWiFiStateMachine &state_machine,
    IWiFiSyncManager &sync_manager)
    : driver_hal_(driver_hal)
    , storage_(storage)
    , state_machine_(state_machine)
    , sync_manager_(sync_manager)
{
}

void WiFiMessageProcessor::process_message(const Message &msg, State state)
{
    if (msg.type == MessageType::COMMAND) {
        if (msg.cmd != CommandId::EXIT) {
            state_machine_.reset_retries();
        }
        switch (msg.cmd) {
        case CommandId::START:
            handle_start(msg, state);
            break;
        case CommandId::STOP:
            handle_stop(msg, state);
            break;
        case CommandId::CONNECT:
            handle_connect(msg, state);
            break;
        case CommandId::DISCONNECT:
            handle_disconnect(msg, state);
            break;
        default:
            break;
        }
    }
    else {
        handle_event(msg, state);
    }
}

void WiFiMessageProcessor::on_idle_tick(State state)
{
    if (state == State::WAITING_RECONNECT) {
        if (storage_.is_valid()) {
            state_machine_.transition_to(State::CONNECTING);
            driver_hal_.wifi_connect();
        }
        else {
            state_machine_.transition_to(State::DISCONNECTED);
        }
    }
}

void WiFiMessageProcessor::handle_start(const Message &msg, State state)
{
    state_machine_.transition_to(State::STARTING);
    esp_err_t err = driver_hal_.wifi_start();
    if (err != ESP_OK) {
        state_machine_.transition_to(state);
        sync_manager_.set_bits(START_FAILED_BIT);
    }
}

void WiFiMessageProcessor::handle_stop(const Message &msg, State state)
{
    state_machine_.transition_to(State::STOPPING);
    esp_err_t err = driver_hal_.wifi_stop();
    if (err != ESP_OK) {
        state_machine_.transition_to(state);
        sync_manager_.set_bits(STOP_FAILED_BIT);
    }
}

void WiFiMessageProcessor::handle_connect(const Message &msg, State state)
{
    state_machine_.transition_to(State::CONNECTING);
    esp_err_t err = driver_hal_.wifi_connect();
    if (err != ESP_OK) {
        state_machine_.transition_to(state);
        sync_manager_.set_bits(CONNECT_FAILED_BIT);
    }
}

void WiFiMessageProcessor::handle_disconnect(const Message &msg, State state)
{
    if (state == State::WAITING_RECONNECT || state == State::CONNECTING) {
        state_machine_.transition_to(State::DISCONNECTED);
        driver_hal_.wifi_disconnect();
        sync_manager_.set_bits(DISCONNECTED_BIT);
        return;
    }
    state_machine_.transition_to(State::DISCONNECTING);
    esp_err_t err = driver_hal_.wifi_disconnect();
    if (err != ESP_OK) {
        state_machine_.transition_to(state);
        sync_manager_.set_bits(CONNECT_FAILED_BIT);
    }
}

void WiFiMessageProcessor::handle_event(const Message &msg, State state)
{
    IWiFiStateMachine::EventOutcome outcome = state_machine_.resolve_event(msg.event);

    if (outcome.next_state != state) {
        state_machine_.transition_to(outcome.next_state);
    }

    if (outcome.bits_to_set != 0) {
        sync_manager_.set_bits(outcome.bits_to_set);
    }

    switch (msg.event) {
    case EventId::STA_DISCONNECTED:
    {
        if (state == State::DISCONNECTING || state == State::STOPPING || !state_machine_.is_active()) {
            sync_manager_.set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT);
            break;
        }
        if (msg.reason == WIFI_REASON_ASSOC_LEAVE) {
            state_machine_.transition_to(State::DISCONNECTED);
            sync_manager_.set_bits(DISCONNECTED_BIT | CONNECT_FAILED_BIT);
            break;
        }

        if (msg.reason == WIFI_REASON_AUTH_FAIL || msg.reason == WIFI_REASON_802_1X_AUTH_FAILED ||
            msg.reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || msg.reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
            msg.reason == WIFI_REASON_CONNECTION_FAIL) {
            if (state_machine_.handle_suspect_failure(msg.rssi)) {
                storage_.save_valid_flag(false);
            }
            else {
                uint32_t delay_ms;
                state_machine_.calculate_next_backoff(delay_ms);
            }
            sync_manager_.set_bits(CONNECT_FAILED_BIT);
            break;
        }
        if (storage_.is_valid()) {
            uint32_t delay_ms;
            state_machine_.calculate_next_backoff(delay_ms);
        }
        else {
            state_machine_.transition_to(State::DISCONNECTED);
        }
        sync_manager_.set_bits(CONNECT_FAILED_BIT);
        break;
    }
    case EventId::GOT_IP:
        state_machine_.reset_retries();
        if (!storage_.is_valid())
            storage_.save_valid_flag(true);
        break;
    default:
        break;
    }
}

} // namespace wifi_manager
