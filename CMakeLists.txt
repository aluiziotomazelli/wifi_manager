# components/wifi_manager/CMakeLists.txt
idf_component_register(
    SRCS 
        "wifi_manager.cpp"
        "wifi_config_storage.cpp"
        "wifi_state_machine.cpp"
        "wifi_driver_hal.cpp"
        "wifi_event_handler.cpp"
        "wifi_sync_manager.cpp"
                    
    INCLUDE_DIRS 
        "include"
                    
    REQUIRES 
        esp_event 
        esp_netif
        esp_timer
        esp_wifi

    PRIV_REQUIRES 
        nvs_flash 
        freertos
)
