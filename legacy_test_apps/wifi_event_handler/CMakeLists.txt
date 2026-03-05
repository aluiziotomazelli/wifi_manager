cmake_minimum_required(VERSION 3.16)

# Add the component directory to the path to include custom components
set(EXTRA_COMPONENT_DIRS "../../../")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_wifi_event_handler)
