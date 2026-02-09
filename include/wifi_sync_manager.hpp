#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

namespace wifi_manager {

/**
 * @class WiFiSyncManager
 * @brief Encapsulates FreeRTOS event groups and queues for WiFiManager synchronization.
 */
class WiFiSyncManager
{
public:
    WiFiSyncManager();
    ~WiFiSyncManager();

    /**
     * @brief Initialize synchronization primitives.
     * @return ESP_OK on success, ESP_ERR_NO_MEM on failure.
     */
    esp_err_t init();

    /**
     * @brief Deinitialize and release resources.
     */
    void deinit();

    /**
     * @brief Post a message to the internal command queue.
     * @param msg The message to post.
     * @return ESP_OK if successful.
     */
    esp_err_t post_message(const Message &msg);

    /**
     * @brief Clear specific synchronization bits.
     * @param bits_to_clear The bits to clear.
     */
    void clear_bits(uint32_t bits_to_clear);

    /**
     * @brief Set specific synchronization bits.
     * @param bits_to_set The bits to set.
     */
    void set_bits(uint32_t bits_to_set);

    /**
     * @brief Wait for specific synchronization bits to be set.
     * @param bits_to_wait The bits to wait for.
     * @param timeout_ms Maximum time to wait in milliseconds.
     * @return The bits that were actually set at the time of return.
     */
    uint32_t wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms);

    /**
     * @brief Check if synchronization primitives are initialized.
     */
    bool is_initialized() const
    {
        return m_command_queue != nullptr && m_event_group != nullptr;
    }

    /**
     * @brief Get the internal queue handle (for task and event handler).
     */
    QueueHandle_t get_queue() const
    {
        return m_command_queue;
    }

    /**
     * @brief Get the internal event group handle.
     */
    EventGroupHandle_t get_event_group() const
    {
        return m_event_group;
    }

private:
    QueueHandle_t m_command_queue;
    EventGroupHandle_t m_event_group;

    static constexpr uint8_t QUEUE_SIZE = 10;
};

} // namespace wifi_manager
