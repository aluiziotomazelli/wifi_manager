// host_test/test_wifi_manager/main/event_stubs.c
#include "esp_event.h"

// WIFI_EVENT and IP_EVENT are declared extern in esp_wifi.h / esp_netif.h
// but their definitions live in ESP-IDF libs not available on host.
// Provide stub definitions so the linker is satisfied.
ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
ESP_EVENT_DEFINE_BASE(IP_EVENT);