# components/wifi_manager/test/CMakeLists.txt
# The following lines of boilerplate have to be in your project's CMakeLists
# file in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "..")

add_compile_definitions(UNIT_TEST)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_wifi_manager)