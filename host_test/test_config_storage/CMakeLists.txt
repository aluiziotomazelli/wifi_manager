cmake_minimum_required(VERSION 3.16)

# Append extra component directories so IDF can find our local components.
list(APPEND EXTRA_COMPONENT_DIRS 
    "../.."                              # The 'ultrasonic_sensor' component being tested
    "../gtest"                           # The GTest wrapper component
    "../common"                          # Mocks
    "$ENV{IDF_PATH}/tools/mocks/esp_wifi"
    "$ENV{IDF_PATH}/tools/mocks/esp_netif"
    "$ENV{IDF_PATH}/tools/mocks/esp_event"
    "$ENV{IDF_PATH}/tools/mocks/lwip"
    "$ENV{IDF_PATH}/tools/mocks/esp_timer"
)

# Explicitly list the components to be included in the build.
set(COMPONENTS main wifi_manager)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_config_storage)


# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_config_storage "*/wifi_manager/src/wifi_config_storage.cpp")

# Set the patterns to remove from the binary
# set(REMOVE_PATTERNS_test_config_storage "*/wifi_manager/include/*")

# Include common coverage logic
 include("../coverage_common.cmake")
 setup_gtest_coverage(test_config_storage test_config_storage.elf)
