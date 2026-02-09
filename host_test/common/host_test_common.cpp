#include "host_test_common.hpp"
#include <string.h>

wifi_config_t g_host_test_wifi_config;
bool g_host_test_auto_simulate_events = true;

// Define event bases
ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
ESP_EVENT_DEFINE_BASE(IP_EVENT);

static esp_err_t stub_esp_wifi_set_config(wifi_interface_t interface, wifi_config_t* conf, int cmock_num_calls) {
    if (conf) {
        memcpy(&g_host_test_wifi_config, conf, sizeof(wifi_config_t));
    }
    return ESP_OK;
}

static esp_err_t stub_esp_wifi_get_config(wifi_interface_t interface, wifi_config_t* conf, int cmock_num_calls) {
    if (conf) {
        memcpy(conf, &g_host_test_wifi_config, sizeof(wifi_config_t));
    }
    return ESP_OK;
}

static esp_err_t stub_esp_wifi_restore(int cmock_num_calls) {
    memset(&g_host_test_wifi_config, 0, sizeof(wifi_config_t));
    return ESP_OK;
}

void host_test_setup_common_mocks(void) {
    memset(&g_host_test_wifi_config, 0, sizeof(wifi_config_t));
    g_host_test_auto_simulate_events = true;

    esp_wifi_init_IgnoreAndReturn(ESP_OK);
    esp_wifi_set_mode_IgnoreAndReturn(ESP_OK);
    esp_wifi_set_config_Stub(stub_esp_wifi_set_config);
    esp_wifi_get_config_Stub(stub_esp_wifi_get_config);
    esp_wifi_restore_Stub(stub_esp_wifi_restore);
    esp_wifi_start_IgnoreAndReturn(ESP_OK);
    esp_wifi_stop_IgnoreAndReturn(ESP_OK);
    esp_wifi_connect_IgnoreAndReturn(ESP_OK);
    esp_wifi_disconnect_IgnoreAndReturn(ESP_OK);
    esp_wifi_deinit_IgnoreAndReturn(ESP_OK);

    esp_netif_init_IgnoreAndReturn(ESP_OK);
    esp_netif_get_handle_from_ifkey_IgnoreAndReturn(NULL);

    esp_event_loop_create_default_IgnoreAndReturn(ESP_OK);
    esp_event_handler_instance_register_IgnoreAndReturn(ESP_OK);
    esp_event_handler_instance_unregister_IgnoreAndReturn(ESP_OK);

    esp_timer_get_time_IgnoreAndReturn(0);
}

esp_netif_t* host_test_manual_esp_netif_create_default_wifi_sta(void) {
    return (esp_netif_t*)0x1234;
}

void host_test_manual_esp_netif_destroy_default_wifi(void *esp_netif) {
    // No-op
}

extern "C" {
// Linker will use these if they are not defined elsewhere
esp_netif_t* esp_netif_create_default_wifi_sta(void) { return host_test_manual_esp_netif_create_default_wifi_sta(); }
void esp_netif_destroy_default_wifi(void *esp_netif) { host_test_manual_esp_netif_destroy_default_wifi(esp_netif); }
}
