cmake_minimum_required(VERSION 3.16)

# Add the component directory to the path to include custom components
set(EXTRA_COMPONENT_DIRS "../../../")

set(COMPONENTS 
        main 
        wifi_manager 
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_integration_internal)
