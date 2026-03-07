#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

/**
 * @file i_wifi_sync_manager.hpp
 * @brief Interface for FreeRTOS synchronization primitives used by WiFiManager.
 */

namespace wifi_manager {

/**
 * @class IWiFiSyncManager
 * @brief Interface for FreeRTOS synchronization primitives used by WiFiManager.
 * @internal
 */
class IWiFiSyncManager
{
public:
    virtual ~IWiFiSyncManager() = default;

    /**
     * @brief Initialize synchronization objects (queue, event group).
     * @internal
     * @return ESP_OK on success.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitialize and delete synchronization objects.
     * @internal
     */
    virtual void deinit() = 0;

    /**
     * @brief Post a message to the internal queue.
     * @internal
     * @param msg Message to post.
     * @return ESP_OK on success.
     */
    virtual esp_err_t post_message(const Message &msg) = 0;

    /**
     * @brief Post a message to the internal queue from an ISR.
     * @internal
     * @param msg Message to post.
     * @return ESP_OK on success.
     */
    virtual esp_err_t post_message_from_isr(const Message &msg) = 0;

    /**
     * @brief Clear bits in the internal event group.
     * @internal
     * @param bits_to_clear Mask of bits to clear.
     */
    virtual void clear_bits(uint32_t bits_to_clear) = 0;

    /**
     * @brief Set bits in the internal event group.
     * @internal
     * @param bits_to_set Mask of bits to set.
     */
    virtual void set_bits(uint32_t bits_to_set) = 0;

    /**
     * @brief Wait for bits to be set in the internal event group.
     * @internal
     * @param bits_to_wait Mask of bits to wait for.
     * @param timeout_ms Maximum time to wait in ms.
     * @return The value of the event group at the time the bits were set or timeout occurred.
     */
    virtual uint32_t wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms) = 0;

    /**
     * @brief Check if synchronization objects are initialized.
     * @internal
     * @return true if initialized.
     */
    virtual bool is_initialized() const = 0;

    /**
     * @brief Get the handle to the message queue.
     * @internal
     * @return QueueHandle_t.
     */
    virtual QueueHandle_t get_queue() const = 0;

    /**
     * @brief Get the handle to the event group.
     * @internal
     * @return EventGroupHandle_t.
     */
    virtual EventGroupHandle_t get_event_group() const = 0;
};

} // namespace wifi_manager
