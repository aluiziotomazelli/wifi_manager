#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"

namespace wifi_manager {

/**
 * @class IWiFiMessageProcessor
 * @brief Interface for processing WiFi manager messages and handling idle logic.
 */
class IWiFiMessageProcessor
{
public:
    virtual ~IWiFiMessageProcessor() = default;

    /**
     * @brief Central dispatcher for all incoming messages.
     * @param msg The message (command or event) to process.
     * @param state The current state of the manager.
     */
    virtual void process_message(const Message &msg, State state) = 0;

    /**
     * @brief Handles idle logic when no messages are pending (e.g., reconnection).
     * @param state The current state of the manager.
     */
    virtual void on_idle_tick(State state) = 0;
};

} // namespace wifi_manager
