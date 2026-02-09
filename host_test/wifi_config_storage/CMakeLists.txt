cmake_minimum_required(VERSION 3.16)

# Include mocks from ESP-IDF
set(EXTRA_COMPONENT_DIRS
    "$ENV{IDF_PATH}/tools/mocks/esp_wifi"
    "$ENV{IDF_PATH}/tools/mocks/esp_netif"
    "$ENV{IDF_PATH}/tools/mocks/esp_event"
    "$ENV{IDF_PATH}/tools/mocks/lwip"
)

# Restrict components to what is strictly necessary
set(COMPONENTS main wifi_manager nvs_flash esp_event esp_netif esp_wifi lwip unity log esp_common esp_system esp_rom soc hal esp_hw_support heap freertos linux esp_partition spi_flash)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(wifi_config_storage_host_test)
