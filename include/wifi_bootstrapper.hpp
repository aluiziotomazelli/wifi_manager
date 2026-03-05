#pragma once

#include "wifi_event_handler.hpp"
#include "interfaces/i_wifi_bootstrapper.hpp"
#include "interfaces/i_wifi_config_storage.hpp"
#include "interfaces/i_wifi_driver_hal.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

namespace wifi_manager {

class WiFiBootstrapper : public IWiFiBootstrapper
{
public:
    WiFiBootstrapper(IWiFiDriverHAL &driver_hal, IWiFiConfigStorage &storage, IWiFiSyncManager &sync_manager);

    ~WiFiBootstrapper() override = default;

    esp_err_t init(TaskFunction_t task_fn, void *pvParameters, TaskHandle_t *pxTaskHandle) override;
    esp_err_t deinit(TaskHandle_t *pxTaskHandle) override;

private:
    IWiFiDriverHAL &driver_hal_;
    IWiFiConfigStorage &storage_;
    IWiFiSyncManager &sync_manager_;

    WiFiEventHandler event_handler_;

    esp_event_handler_instance_t wifi_event_instance_ = nullptr;
    esp_event_handler_instance_t ip_event_instance_ = nullptr;
    esp_netif_t *sta_netif_ = nullptr;
};

} // namespace wifi_manager
