#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "interfaces/i_wifi_sync_manager.hpp"

namespace wifi_manager {

/**
 * @class WiFiSyncManager
 * @brief Encapsulates FreeRTOS event groups and queues for WiFiManager synchronization.
 */
class WiFiSyncManager : public IWiFiSyncManager
{
public:
    WiFiSyncManager();
    ~WiFiSyncManager() override;

    /**
     * @brief Initialize synchronization primitives.
     * @return ESP_OK on success, ESP_ERR_NO_MEM on failure.
     */
    esp_err_t init() override;

    /**
     * @brief Deinitialize and release resources.
     */
    void deinit() override;

    /**
     * @brief Post a message to the internal command queue.
     * @param msg The message to post.
     * @return ESP_OK if successful.
     */
    esp_err_t post_message(const Message &msg) override;
    esp_err_t post_message_from_isr(const Message &msg) override; // TODO: is this needed?

    /**
     * @brief Clear specific synchronization bits.
     * @param bits_to_clear The bits to clear.
     */
    void clear_bits(uint32_t bits_to_clear) override;

    /**
     * @brief Set specific synchronization bits.
     * @param bits_to_set The bits to set.
     */
    void set_bits(uint32_t bits_to_set) override;

    /**
     * @brief Wait for specific synchronization bits to be set.
     * @param bits_to_wait The bits to wait for.
     * @param timeout_ms Maximum time to wait in milliseconds.
     * @return The bits that were actually set at the time of return.
     */
    uint32_t wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms) override;

    /**
     * @brief Check if synchronization primitives are initialized.
     */
    bool is_initialized() const override { return command_queue_ != nullptr && event_group_ != nullptr; }

    /**
     * @brief Get the internal queue handle (for task and event handler).
     */
    QueueHandle_t get_queue() const override { return command_queue_; }

    /**
     * @brief Get the internal event group handle.
     */
    EventGroupHandle_t get_event_group() const override { return event_group_; }

private:
    QueueHandle_t command_queue_;
    EventGroupHandle_t event_group_;

    static constexpr uint8_t QUEUE_SIZE = 10;
};

} // namespace wifi_manager
