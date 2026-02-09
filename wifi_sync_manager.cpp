#include "wifi_sync_manager.hpp"
#include "esp_log.h"

namespace wifi_manager {

static const char *TAG = "WiFiSyncManager";

WiFiSyncManager::WiFiSyncManager()
    : m_command_queue(nullptr)
    , m_event_group(nullptr)
{
}

WiFiSyncManager::~WiFiSyncManager()
{
    deinit();
}

esp_err_t WiFiSyncManager::init()
{
    if (m_command_queue == nullptr) {
        m_command_queue = xQueueCreate(QUEUE_SIZE, sizeof(Message));
        if (m_command_queue == nullptr) {
            ESP_LOGE(TAG, "Failed to create command queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (m_event_group == nullptr) {
        m_event_group = xEventGroupCreate();
        if (m_event_group == nullptr) {
            ESP_LOGE(TAG, "Failed to create event group");
            vQueueDelete(m_command_queue);
            m_command_queue = nullptr;
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

void WiFiSyncManager::deinit()
{
    if (m_command_queue != nullptr) {
        vQueueDelete(m_command_queue);
        m_command_queue = nullptr;
    }

    if (m_event_group != nullptr) {
        vEventGroupDelete(m_event_group);
        m_event_group = nullptr;
    }
}

esp_err_t WiFiSyncManager::post_message(const Message &msg)
{
    if (m_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(m_command_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Command queue full, failed to post message");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void WiFiSyncManager::clear_bits(uint32_t bits_to_clear)
{
    if (m_event_group != nullptr) {
        xEventGroupClearBits(m_event_group, bits_to_clear);
    }
}

void WiFiSyncManager::set_bits(uint32_t bits_to_set)
{
    if (m_event_group != nullptr) {
        xEventGroupSetBits(m_event_group, bits_to_set);
    }
}

uint32_t WiFiSyncManager::wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms)
{
    if (m_event_group == nullptr) {
        return 0;
    }

    return xEventGroupWaitBits(m_event_group, bits_to_wait, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
}

} // namespace wifi_manager
