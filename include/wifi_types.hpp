#pragma once

#include <cstdint>

/**
 * @file wifi_types.hpp
 * @brief Common types and messages for the WiFiManager component.
 */

namespace wifi_manager {

/**
 * @enum State
 * @brief Internal states of the WiFi Manager.
 */
enum class State : uint8_t
{
    UNINITIALIZED     = 0, ///< Initial state before init()
    INITIALIZING      = 1, ///< init() in progress
    INITIALIZED       = 2, ///< Task running, ready for commands
    STARTING          = 3, ///< wifi_start() in progress
    STARTED           = 4, ///< WiFi driver active, not connected
    CONNECTING        = 5, ///< wifi_connect() in progress
    CONNECTED_NO_IP   = 6, ///< Connected to AP, waiting for IP
    CONNECTED_GOT_IP  = 7, ///< Fully connected with valid IP
    DISCONNECTING     = 8, ///< wifi_disconnect() in progress
    WAITING_RECONNECT = 9, ///< Waiting for backoff timer to retry
    ERROR_CREDENTIALS = 10, ///< Connection failed due to wrong credentials
    STOPPING          = 11, ///< wifi_stop() in progress
    COUNT             = 12, ///< Number of states

    DISCONNECTED = STARTED,     ///< Alias for STARTED state
    STOPPED      = INITIALIZED, ///< Alias for INITIALIZED state
};

/**
 * @enum CommandId
 * @brief Public commands that can be sent to the WiFi Manager.
 */
enum class CommandId : uint8_t
{
    START,      ///< Start the WiFi driver
    STOP,       ///< Stop the WiFi driver
    CONNECT,    ///< Connect to the Access Point
    DISCONNECT, ///< Disconnect from the Access Point
    EXIT,       ///< Internal: terminate the background task
    COUNT       ///< Number of commands
};

/**
 * @enum EventId
 * @brief Internal events reported by the ESP-IDF driver.
 */
enum class EventId : uint8_t
{
    STA_START,        ///< WiFi station started
    STA_STOP,         ///< WiFi station stopped
    STA_CONNECTED,    ///< WiFi station connected to AP
    STA_DISCONNECTED, ///< WiFi station disconnected from AP
    GOT_IP,           ///< Obtained IP address
    LOST_IP,          ///< Lost IP address
    COUNT             ///< Number of events
};

/**
 * @enum MessageType
 * @brief Discriminator for the internal message queue.
 */
enum class MessageType : uint8_t
{
    COMMAND, ///< Action requested by the user/API
    EVENT,   ///< Signal reported by the system
};

/**
 * @struct Message
 * @brief Structure used to pass commands and events to the internal task.
 */
struct Message
{
    MessageType type; ///< Type of message (Command or Event)
    union
    {
        CommandId cmd;   ///< Command ID (if type is COMMAND)
        EventId event;   ///< Event ID (if type is EVENT)
    };
    uint8_t reason; ///< Reason code (for STA_DISCONNECTED)
    int8_t rssi;    ///< RSSI level (for STA_DISCONNECTED)
};

// FreeRTOS Event Group bits for synchronization between the API and the task
static constexpr uint32_t STARTED_BIT        = (1 << 0); ///< WiFi driver started
static constexpr uint32_t STOPPED_BIT        = (1 << 1); ///< WiFi driver stopped
static constexpr uint32_t CONNECTED_BIT      = (1 << 2); ///< Got IP address
static constexpr uint32_t DISCONNECTED_BIT   = (1 << 3); ///< Disconnected from AP
static constexpr uint32_t CONNECT_FAILED_BIT = (1 << 4); ///< Connection attempt failed
static constexpr uint32_t START_FAILED_BIT   = (1 << 5); ///< Driver start failed
static constexpr uint32_t STOP_FAILED_BIT    = (1 << 6); ///< Driver stop failed
static constexpr uint32_t INVALID_STATE_BIT  = (1 << 7); ///< Invalid state for requested command

/**
 * @brief Mask containing all synchronization bits.
 */
static constexpr uint32_t ALL_SYNC_BITS = STARTED_BIT | STOPPED_BIT | CONNECTED_BIT | DISCONNECTED_BIT |
                                          CONNECT_FAILED_BIT | START_FAILED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT;

} // namespace wifi_manager
