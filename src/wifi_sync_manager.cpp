#include "wifi_sync_manager.hpp"
#include "esp_log.h"

namespace wifi_manager {

static const char *TAG = "WiFiSyncManager";

WiFiSyncManager::WiFiSyncManager()
    : command_queue_(nullptr)
    , event_group_(nullptr)
{
}

WiFiSyncManager::~WiFiSyncManager()
{
    deinit();
}

esp_err_t WiFiSyncManager::init()
{
    if (command_queue_ == nullptr) {
        command_queue_ = xQueueCreate(QUEUE_SIZE, sizeof(Message));
        if (command_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create command queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (event_group_ == nullptr) {
        event_group_ = xEventGroupCreate();
        if (event_group_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create event group");
            vQueueDelete(command_queue_);
            command_queue_ = nullptr;
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

void WiFiSyncManager::deinit()
{
    if (command_queue_ != nullptr) {
        vQueueDelete(command_queue_);
        command_queue_ = nullptr;
    }

    if (event_group_ != nullptr) {
        vEventGroupDelete(event_group_);
        event_group_ = nullptr;
    }
}

esp_err_t WiFiSyncManager::post_message(const Message &msg)
{
    if (command_queue_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(command_queue_, &msg, 0) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t WiFiSyncManager::post_message_from_isr(const Message &msg)
{
    if (command_queue_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSendFromISR(command_queue_, &msg, nullptr) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

void WiFiSyncManager::clear_bits(uint32_t bits_to_clear)
{
    if (event_group_ != nullptr) {
        xEventGroupClearBits(event_group_, bits_to_clear);
    }
}

void WiFiSyncManager::set_bits(uint32_t bits_to_set)
{
    if (event_group_ != nullptr) {
        xEventGroupSetBits(event_group_, bits_to_set);
    }
}

uint32_t WiFiSyncManager::wait_for_bits(uint32_t bits_to_wait, uint32_t timeout_ms)
{
    if (event_group_ == nullptr) {
        return 0;
    }

    return xEventGroupWaitBits(event_group_, bits_to_wait, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
}

} // namespace wifi_manager
