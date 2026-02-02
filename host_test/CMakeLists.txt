cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main wifi_manager)

# Add the directory containing the component under test
list(APPEND EXTRA_COMPONENT_DIRS "..")

# No host, precisamos de mocks para o que não é simulado
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/freertos")

# Se estivéssemos na v5.5.2, incluiríamos:
# list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/esp_wifi")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_wifi_manager_host)
