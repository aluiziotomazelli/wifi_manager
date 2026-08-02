#pragma once

#include "wifi_event_handler.hpp"
#include "interfaces/i_wifi_bootstrapper.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

/**
 * @file wifi_bootstrapper.hpp
 * @brief Concrete implementation of IWiFiBootstrapper.
 */

namespace wifi_manager {

/**
 * @class WiFiBootstrapper
 * @brief Concrete implementation of IWiFiBootstrapper responsible for component orchestration.
 */
class WiFiBootstrapper : public IWiFiBootstrapper
{
public:
    /**
     * @brief Construct a new WiFiBootstrapper.
     *
     * @param driver_hal Reference to the WiFi driver HAL.
     * @param storage Reference to the configuration storage.
     * @param sync_manager Reference to the synchronization manager.
     */
    WiFiBootstrapper(IWiFiDriverHAL &driver_hal, IWiFiConfigStorage &storage, IWiFiSyncManager &sync_manager);

    ~WiFiBootstrapper() override = default;

    /**
     * @copydoc IWiFiBootstrapper::init()
     */
    esp_err_t init(
        TaskFunction_t task_fn,
        void *pvParameters,
        TaskHandle_t *pxTaskHandle,
        uint32_t task_stack_size = 4096,
        UBaseType_t task_priority = 5) override;

    /**
     * @copydoc IWiFiBootstrapper::deinit()
     */
    esp_err_t deinit(TaskHandle_t *pxTaskHandle) override;

private:
    IWiFiDriverHAL &driver_hal_;     ///< Reference to the WiFi driver HAL
    IWiFiConfigStorage &storage_;    ///< Reference to the configuration storage
    IWiFiSyncManager &sync_manager_; ///< Reference to the synchronization manager

    WiFiEventHandler event_handler_; ///< Internal event handler instance

    esp_event_handler_instance_t wifi_event_instance_ = nullptr; ///< WiFi event handler instance
    esp_event_handler_instance_t ip_event_instance_ = nullptr;   ///< IP event handler instance
    esp_netif_t *sta_netif_ = nullptr;                           ///< Network interface handle
};

} // namespace wifi_manager
