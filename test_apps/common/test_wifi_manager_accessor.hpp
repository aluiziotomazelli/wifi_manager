#pragma once

#include "esp_wifi.h"
#include "wifi_event_handler.hpp"
#include "wifi_manager.hpp"
#include "wifi_sync_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @class WiFiManagerTestAccessor
 * @brief Friend class to allow testing internal members of WiFiManager.
 */
class WiFiManagerTestAccessor
{
private:
    WiFiManager &wifi_manager;

public:
    explicit WiFiManagerTestAccessor(WiFiManager &wm)
        : wifi_manager(wm)
    {
    }

    // === Test Interface via Direct Member Access ===

    /**
     * @brief Suspend the internal WiFiManager task.
     */
    void test_suspend_manager_task()
    {
        if (wifi_manager.task_handle != nullptr) {
            vTaskSuspend(wifi_manager.task_handle);
        }
    }

    /**
     * @brief Resume the internal WiFiManager task.
     */
    void test_resume_manager_task()
    {
        if (wifi_manager.task_handle != nullptr) {
            vTaskResume(wifi_manager.task_handle);
        }
    }

    /**
     * @brief Send a start command directly to the internal queue.
     */
    esp_err_t test_send_start_command(bool is_async = true)
    {
        wifi_manager::Message msg = {};
        msg.type                  = wifi_manager::MessageType::COMMAND;
        msg.cmd                   = wifi_manager::CommandId::START;
        return wifi_manager.post_message(msg, is_async);
    }

    /**
     * @brief Send a stop command directly to the internal queue.
     */
    esp_err_t test_send_stop_command(bool is_async = true)
    {
        wifi_manager::Message msg = {};
        msg.type                  = wifi_manager::MessageType::COMMAND;
        msg.cmd                   = wifi_manager::CommandId::STOP;
        return wifi_manager.post_message(msg, is_async);
    }

    /**
     * @brief Send a connect command directly to the internal queue.
     */
    esp_err_t test_send_connect_command(bool is_async = true)
    {
        wifi_manager::Message msg = {};
        msg.type                  = wifi_manager::MessageType::COMMAND;
        msg.cmd                   = wifi_manager::CommandId::CONNECT;
        return wifi_manager.post_message(msg, is_async);
    }

    /**
     * @brief Send a disconnect command directly to the internal queue.
     */
    esp_err_t test_send_disconnect_command(bool is_async = true)
    {
        wifi_manager::Message msg = {};
        msg.type                  = wifi_manager::MessageType::COMMAND;
        msg.cmd                   = wifi_manager::CommandId::DISCONNECT;
        return wifi_manager.post_message(msg, is_async);
    }

    /**
     * @brief Get the current internal state of the manager.
     */
    WiFiManager::State test_get_internal_state()
    {
        return wifi_manager.get_state();
    }

    /**
     * @brief Get the number of pending commands in the queue.
     */
    uint32_t test_get_queue_pending_count()
    {
        if (!wifi_manager.sync_manager.is_initialized())
            return 0;
        return uxQueueMessagesWaiting(wifi_manager.sync_manager.get_queue());
    }

    /**
     * @brief Check if the command queue is full.
     */
    bool test_is_queue_full()
    {
        if (!wifi_manager.sync_manager.is_initialized())
            return true;
        return uxQueueSpacesAvailable(wifi_manager.sync_manager.get_queue()) == 0;
    }

    /**
     * @brief Simulate a WiFi disconnection event with RSSI.
     */
    void test_simulate_disconnect(uint8_t reason, int8_t rssi = -60)
    {
        wifi_event_sta_disconnected_t disconn = {};
        disconn.reason                        = reason;
        disconn.rssi                          = rssi;

        wifi_manager::WiFiEventHandler::wifi_event_handler(wifi_manager.sync_manager.get_queue(), WIFI_EVENT,
                                                           WIFI_EVENT_STA_DISCONNECTED, &disconn);
    }

    /**
     * @brief Simulate a WiFi event.
     */
    void test_simulate_wifi_event(int32_t id, void *data = nullptr)
    {
        wifi_manager::WiFiEventHandler::wifi_event_handler(wifi_manager.sync_manager.get_queue(), WIFI_EVENT, id, data);
    }

    /**
     * @brief Simulate an IP event.
     */
    void test_simulate_ip_event(int32_t id, void *data = nullptr)
    {
        wifi_manager::WiFiEventHandler::ip_event_handler(wifi_manager.sync_manager.get_queue(), IP_EVENT, id, data);
    }
};
