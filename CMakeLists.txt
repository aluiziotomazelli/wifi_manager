# components/wifi_manager/CMakeLists.txt
idf_component_register(
    NAME wifi_manager
    SRCS 
        "src/wifi_manager.cpp"
        "src/wifi_message_processor.cpp"
        "src/wifi_config_storage.cpp"
        "src/wifi_state_machine.cpp"
        "src/wifi_event_handler.cpp"
        "src/wifi_sync_manager.cpp"
        "src/wifi_bootstrapper.cpp"
                    
    INCLUDE_DIRS 
        "include"
        "include/interfaces"
        "include/mocks"
                    
    REQUIRES 
        esp_event 
        esp_netif
        esp_timer
        esp_wifi

    PRIV_REQUIRES 
        nvs_flash 
        freertos
)
