#pragma once

#include <cstdint>

/**
 * @file wifi_types.hpp
 * @brief Common types and messages for the WiFiManager component.
 */

namespace wifi_manager {

enum class State : uint8_t
{
    UNINITIALIZED     = 0,
    INITIALIZING      = 1,
    INITIALIZED       = 2,
    STARTING          = 3,
    STARTED           = 4,
    CONNECTING        = 5,
    CONNECTED_NO_IP   = 6,
    CONNECTED_GOT_IP  = 7,
    DISCONNECTING     = 8,
    WAITING_RECONNECT = 9,
    ERROR_CREDENTIALS = 10,
    STOPPING          = 11,
    COUNT             = 12,

    DISCONNECTED = STARTED,
    STOPPED      = INITIALIZED,
};

enum class CommandId : uint8_t
{
    START,
    STOP,
    CONNECT,
    DISCONNECT,
    EXIT,
    COUNT
};

enum class EventId : uint8_t
{
    STA_START,
    STA_STOP,
    STA_CONNECTED,
    STA_DISCONNECTED,
    GOT_IP,
    LOST_IP,
    COUNT
};

/**
 * @brief Discriminator for the internal message queue.
 */
enum class MessageType : uint8_t
{
    COMMAND, ///< Action requested by the user/API
    EVENT,   ///< Signal reported by the system
};

/**
 * @brief Structure used to pass commands and events to the internal task.
 */
struct Message
{
    MessageType type;
    union
    {
        CommandId cmd;
        EventId event;
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
static constexpr uint32_t INVALID_STATE_BIT  = (1 << 7); ///< Invalid state

static constexpr uint32_t ALL_SYNC_BITS = STARTED_BIT | STOPPED_BIT | CONNECTED_BIT | DISCONNECTED_BIT |
                                          CONNECT_FAILED_BIT | START_FAILED_BIT | STOP_FAILED_BIT | INVALID_STATE_BIT;

} // namespace wifi_manager
