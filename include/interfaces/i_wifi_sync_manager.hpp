#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

namespace wifi_manager {

/**
 * @class IWiFiSyncManager
 * @brief Interface for FreeRTOS synchronization primitives used by WiFiManager.
 */
class IWiFiSyncManager
{
public:
    virtual ~IWiFiSyncManager() = default;

    virtual esp_err_t init() = 0;
    virtual void deinit() = 0;

    virtual esp_err_t post_message(const Message &msg) = 0;
    virtual esp_err_t post_message_from_isr(const Message &msg) = 0;

    virtual void clear_bits(uint32_t bits_to_clear) = 0;
    virtual void set_bits(uint32_t bits_to_set) = 0;
    virtual uint32_t wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms) = 0;

    virtual bool is_initialized() const = 0;
    virtual QueueHandle_t get_queue() const = 0;
    virtual EventGroupHandle_t get_event_group() const = 0;
};

} // namespace wifi_manager
