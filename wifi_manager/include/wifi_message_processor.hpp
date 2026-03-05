#pragma once

#include "interfaces/i_wifi_message_processor.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_state_machine.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

namespace wifi_manager {

/**
 * @class WiFiMessageProcessor
 * @brief Implementation of message processing logic for WiFi Manager.
 */
class WiFiMessageProcessor : public IWiFiMessageProcessor
{
public:
    WiFiMessageProcessor(
        IWiFiDriverHAL &driver_hal,
        IWiFiConfigStorage &storage,
        IWiFiStateMachine &state_machine,
        IWiFiSyncManager &sync_manager);

    void process_message(const Message &msg, State state) override;
    void on_idle_tick(State state) override;

private:
    void handle_start(const Message &msg, State state);
    void handle_stop(const Message &msg, State state);
    void handle_connect(const Message &msg, State state);
    void handle_disconnect(const Message &msg, State state);
    void handle_event(const Message &msg, State state);

    IWiFiDriverHAL &driver_hal_;
    IWiFiConfigStorage &storage_;
    IWiFiStateMachine &state_machine_;
    IWiFiSyncManager &sync_manager_;
};

} // namespace wifi_manager
