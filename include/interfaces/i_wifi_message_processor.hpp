#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"

/**
 * @file i_wifi_message_processor.hpp
 * @brief Interface for processing WiFi manager messages and handling idle logic.
 */

namespace wifi_manager {

/**
 * @class IWiFiMessageProcessor
 * @brief Interface for processing WiFi manager messages and handling idle logic.
 * @internal
 */
class IWiFiMessageProcessor
{
public:
    virtual ~IWiFiMessageProcessor() = default;

    /**
     * @brief Central dispatcher for all incoming messages.
     * @internal
     * @param msg The message (command or event) to process.
     * @param state The current state of the manager.
     */
    virtual void process_message(const Message &msg, State state) = 0;

    /**
     * @brief Handles idle logic when no messages are pending (e.g., reconnection).
     * @internal
     * @param state The current state of the manager.
     */
    virtual void on_idle_tick(State state) = 0;
};

} // namespace wifi_manager
