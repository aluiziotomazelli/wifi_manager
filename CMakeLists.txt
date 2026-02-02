# components/wifi_manager/CMakeLists.txt
idf_component_register(
    SRCS 
        "wifi_manager.cpp"
                    
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
        # common
)
