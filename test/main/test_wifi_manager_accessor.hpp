// test_wifi_accessor.hpp
#pragma once

#ifdef UNIT_TEST

#include "esp_wifi.h"
#include "wifi_manager.hpp"

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

    // === Test Interface via Helpers ===

    /**
     * @brief Send a start command directly to the internal queue.
     */
    esp_err_t test_send_start_command(bool is_async = true)
    {
        return wifi_manager.test_helper_send_start_command(is_async);
    }

    /**
     * @brief Send a stop command directly to the internal queue.
     */
    esp_err_t test_send_stop_command(bool is_async = true)
    {
        return wifi_manager.test_helper_send_stop_command(is_async);
    }

    /**
     * @brief Send a connect command directly to the internal queue.
     */
    esp_err_t test_send_connect_command(bool is_async = true)
    {
        return wifi_manager.test_helper_send_connect_command(is_async);
    }

    /**
     * @brief Send a disconnect command directly to the internal queue.
     */
    esp_err_t test_send_disconnect_command(bool is_async = true)
    {
        return wifi_manager.test_helper_send_disconnect_command(is_async);
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
        return wifi_manager.test_helper_get_queue_pending_count();
    }

    /**
     * @brief Check if the command queue is full.
     */
    bool test_is_queue_full()
    {
        return wifi_manager.test_helper_is_queue_full();
    }

    /**
     * @brief Simulate a WiFi disconnection event.
     */
    void test_simulate_disconnect(uint8_t reason)
    {
        wifi_event_sta_disconnected_t disconn = {};
        disconn.reason                        = reason;
        WiFiManager::wifi_event_handler(&wifi_manager, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                        &disconn);
    }

    /**
     * @brief Simulate a WiFi event.
     */
    void test_simulate_wifi_event(int32_t id, void *data = nullptr)
    {
        WiFiManager::wifi_event_handler(&wifi_manager, WIFI_EVENT, id, data);
    }

    /**
     * @brief Simulate an IP event.
     */
    void test_simulate_ip_event(int32_t id, void *data = nullptr)
    {
        WiFiManager::ip_event_handler(&wifi_manager, IP_EVENT, id, data);
    }

    /**
     * @brief Get the total capacity of the command queue.
     */
    uint32_t test_get_queue_capacity()
    {
        uint32_t pending = wifi_manager.test_helper_get_queue_pending_count();
        uint32_t free    = wifi_manager.test_helper_is_queue_full() ? 0 : 10 - pending;
        return pending + free; // Total capacity = 10
    }
};

#endif // UNIT_TEST
