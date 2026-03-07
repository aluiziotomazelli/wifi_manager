cmake_minimum_required(VERSION 3.16)

# Append extra component directories so IDF can find our local components.
list(APPEND EXTRA_COMPONENT_DIRS
    "../.."
    "../gtest"
    "../common"
    "$ENV{IDF_PATH}/tools/mocks/esp_wifi"
    "$ENV{IDF_PATH}/tools/mocks/esp_netif"
    "$ENV{IDF_PATH}/tools/mocks/esp_event"
    "$ENV{IDF_PATH}/tools/mocks/lwip"
    "$ENV{IDF_PATH}/tools/mocks/esp_timer"
)

# Explicitly list the components to be included in the build.
set(COMPONENTS main wifi_manager)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_bootstrapper)


# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_bootstrapper "*/wifi_manager/src/wifi_bootstrapper.cpp")

# Set the patterns to remove from the binary
# set(REMOVE_PATTERNS_test_bootstrapper 
#     "*/wifi_event_handler.cpp"
#     "*/wifi_event_handler*"
#     "*wifi_event_handler*"
# )

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_bootstrapper test_bootstrapper.elf)
