idf_component_register(
    SRCS
        "main.cpp"
        "test_wifi_manager_init.cpp"
        "event_stubs.c"
    INCLUDE_DIRS
        "."
        "../../common"
    REQUIRES
        gtest
        wifi_manager
    WHOLE_ARCHIVE
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()