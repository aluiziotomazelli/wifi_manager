# components/wifi_manager/CMakeLists.txt
idf_component_register(
    SRCS 
        "src/wifi_manager.cpp"
        "src/wifi_config_storage.cpp"
        "src/wifi_state_machine.cpp"
        "src/wifi_driver_hal.cpp"
        "src/wifi_event_handler.cpp"
        "src/wifi_sync_manager.cpp"
                    
    INCLUDE_DIRS 
        "include"
        "include/interfaces"
                    
    REQUIRES 
        esp_event 
        esp_netif
        esp_timer
        esp_wifi

    PRIV_REQUIRES 
        nvs_flash 
        freertos
)
