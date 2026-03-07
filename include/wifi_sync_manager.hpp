#pragma once

#include "esp_err.h"
#include "wifi_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "interfaces/i_wifi_sync_manager.hpp"

/**
 * @file wifi_sync_manager.hpp
 * @brief Concrete implementation of IWiFiSyncManager using FreeRTOS.
 */

namespace wifi_manager {

/**
 * @class WiFiSyncManager
 * @brief Encapsulates FreeRTOS event groups and queues for WiFiManager synchronization.
 */
class WiFiSyncManager : public IWiFiSyncManager
{
public:
    /**
     * @brief Construct a new WiFiSyncManager.
     */
    WiFiSyncManager();

    ~WiFiSyncManager() override;

    /**
     * @copydoc IWiFiSyncManager::init()
     */
    esp_err_t init() override;

    /**
     * @copydoc IWiFiSyncManager::deinit()
     */
    void deinit() override;

    /**
     * @copydoc IWiFiSyncManager::post_message()
     */
    esp_err_t post_message(const Message &msg) override;

    /**
     * @copydoc IWiFiSyncManager::post_message_from_isr()
     */
    esp_err_t post_message_from_isr(const Message &msg) override;

    /**
     * @copydoc IWiFiSyncManager::clear_bits()
     */
    void clear_bits(uint32_t bits_to_clear) override;

    /**
     * @copydoc IWiFiSyncManager::set_bits()
     */
    void set_bits(uint32_t bits_to_set) override;

    /**
     * @copydoc IWiFiSyncManager::wait_for_bits()
     */
    uint32_t wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms) override;

    /**
     * @copydoc IWiFiSyncManager::is_initialized()
     */
    bool is_initialized() const override { return command_queue_ != nullptr && event_group_ != nullptr; }

    /**
     * @copydoc IWiFiSyncManager::get_queue()
     */
    QueueHandle_t get_queue() const override { return command_queue_; }

    /**
     * @copydoc IWiFiSyncManager::get_event_group()
     */
    EventGroupHandle_t get_event_group() const override { return event_group_; }

private:
    QueueHandle_t command_queue_;    ///< Handle for the message queue
    EventGroupHandle_t event_group_; ///< Handle for the event group

    static constexpr uint8_t QUEUE_SIZE = 10; ///< Size of the message queue
};

} // namespace wifi_manager
