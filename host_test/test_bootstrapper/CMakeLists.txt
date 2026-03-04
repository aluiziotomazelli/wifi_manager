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

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_bootstrapper test_bootstrapper.elf)
