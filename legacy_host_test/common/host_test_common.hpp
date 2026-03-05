#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "Mockesp_wifi.h"
#include "Mockesp_netif.h"
#include "Mockesp_event.h"
#include "Mockesp_timer.h"

/**
 * @brief Initialize all common mocks with default successful behaviors.
 * This can be called from setUp().
 */
void host_test_setup_common_mocks(void);

/**
 * @brief Manual mock for esp_netif_create_default_wifi_sta.
 */
esp_netif_t* host_test_manual_esp_netif_create_default_wifi_sta(void);

/**
 * @brief Manual mock for esp_netif_destroy_default_wifi.
 */
void host_test_manual_esp_netif_destroy_default_wifi(void *esp_netif);

/**
 * @brief Global Wi-Fi configuration storage for stubs.
 */
extern wifi_config_t g_host_test_wifi_config;

/**
 * @brief Control whether stubs should automatically trigger events.
 */
extern bool g_host_test_auto_simulate_events;

#ifdef __cplusplus
}
#endif
