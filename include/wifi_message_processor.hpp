#pragma once

#include "interfaces/i_wifi_message_processor.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_state_machine.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

/**
 * @file wifi_message_processor.hpp
 * @brief Concrete implementation of IWiFiMessageProcessor.
 */

namespace wifi_manager {

/**
 * @class WiFiMessageProcessor
 * @brief Implementation of message processing logic for WiFi Manager.
 */
class WiFiMessageProcessor : public IWiFiMessageProcessor
{
public:
    /**
     * @brief Construct a new WiFiMessageProcessor.
     *
     * @param driver_hal Reference to the WiFi driver HAL.
     * @param storage Reference to the configuration storage.
     * @param state_machine Reference to the state machine.
     * @param sync_manager Reference to the synchronization manager.
     */
    WiFiMessageProcessor(
        IWiFiDriverHAL &driver_hal,
        IWiFiConfigStorage &storage,
        IWiFiStateMachine &state_machine,
        IWiFiSyncManager &sync_manager);

    /**
     * @copydoc IWiFiMessageProcessor::process_message()
     */
    void process_message(const Message &msg, State state) override;

    /**
     * @copydoc IWiFiMessageProcessor::on_idle_tick()
     */
    void on_idle_tick(State state) override;

private:
    /**
     * @brief Handle START command.
     * @param msg Message object.
     * @param state Current state.
     */
    void handle_start(const Message &msg, State state);

    /**
     * @brief Handle STOP command.
     * @param msg Message object.
     * @param state Current state.
     */
    void handle_stop(const Message &msg, State state);

    /**
     * @brief Handle CONNECT command.
     * @param msg Message object.
     * @param state Current state.
     */
    void handle_connect(const Message &msg, State state);

    /**
     * @brief Handle DISCONNECT command.
     * @param msg Message object.
     * @param state Current state.
     */
    void handle_disconnect(const Message &msg, State state);

    /**
     * @brief Handle system events.
     * @param msg Message object containing event data.
     * @param state Current state.
     */
    void handle_event(const Message &msg, State state);

    IWiFiDriverHAL &driver_hal_;     ///< Reference to the WiFi driver HAL
    IWiFiConfigStorage &storage_;    ///< Reference to the configuration storage
    IWiFiStateMachine &state_machine_; ///< Reference to the state machine
    IWiFiSyncManager &sync_manager_;   ///< Reference to the synchronization manager
};

} // namespace wifi_manager
