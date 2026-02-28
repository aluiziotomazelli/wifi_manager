#include "wifi_driver_hal.hpp"
#include "esp_log.h"

static const char *TAG = "WiFiDriverHAL";

WiFiDriverHAL::WiFiDriverHAL()
    : m_sta_netif(nullptr)
    , m_wifi_event_instance(nullptr)
    , m_ip_event_instance(nullptr)
    , m_wifi_init_done(false)
{
}

WiFiDriverHAL::~WiFiDriverHAL()
{
    deinit();
}

esp_err_t WiFiDriverHAL::init_netif()
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_netif_init: %s", esp_err_to_name(err));
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Netif already initialized.");
    }
    return ESP_OK;
}

esp_err_t WiFiDriverHAL::create_default_event_loop()
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        return err;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop already created.");
    }
    return ESP_OK;
}

esp_err_t WiFiDriverHAL::setup_sta_netif()
{
    m_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (m_sta_netif == nullptr) {
        m_sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (m_sta_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create default STA netif");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t WiFiDriverHAL::init_wifi()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err          = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to esp_wifi_init: %s", esp_err_to_name(err));
        return err;
    }
    if (err == ESP_OK) {
        m_wifi_init_done = true;
    }
    return ESP_OK;
}

esp_err_t WiFiDriverHAL::set_mode_sta()
{
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

esp_err_t WiFiDriverHAL::register_event_handlers(esp_event_handler_t wifi_handler,
                                                 esp_event_handler_t ip_handler,
                                                 void *handler_arg)
{
    esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, handler_arg,
                                                        &m_wifi_event_instance);
    if (err != ESP_OK)
        return err;

    err =
        esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_handler, handler_arg, &m_ip_event_instance);
    return err;
}

esp_err_t WiFiDriverHAL::unregister_event_handlers()
{
    if (m_wifi_event_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, m_wifi_event_instance);
        m_wifi_event_instance = nullptr;
    }
    if (m_ip_event_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, m_ip_event_instance);
        m_ip_event_instance = nullptr;
    }
    return ESP_OK;
}

esp_err_t WiFiDriverHAL::start()
{
    return esp_wifi_start();
}

esp_err_t WiFiDriverHAL::stop()
{
    return esp_wifi_stop();
}

esp_err_t WiFiDriverHAL::connect()
{
    return esp_wifi_connect();
}

esp_err_t WiFiDriverHAL::disconnect()
{
    return esp_wifi_disconnect();
}

esp_err_t WiFiDriverHAL::restore()
{
    return esp_wifi_restore();
}

esp_err_t WiFiDriverHAL::set_config(wifi_config_t *cfg)
{
    return esp_wifi_set_config(WIFI_IF_STA, cfg);
}

esp_err_t WiFiDriverHAL::get_config(wifi_config_t *cfg)
{
    return esp_wifi_get_config(WIFI_IF_STA, cfg);
}

esp_err_t WiFiDriverHAL::deinit()
{
    esp_err_t err = ESP_OK;

    if (m_wifi_init_done) {
        err = esp_wifi_deinit();
        if (err == ESP_OK || err == ESP_ERR_WIFI_NOT_INIT) {
            m_wifi_init_done = false;
        }
    }

    if (m_sta_netif) {
        esp_netif_destroy_default_wifi(m_sta_netif);
        m_sta_netif = nullptr;
    }

    return err;
}
