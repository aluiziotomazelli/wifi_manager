idf_component_register(
    SRCS
        "main.cpp"
        "test_config_storage.cpp"
    INCLUDE_DIRS
        "."
        "../../common"
    REQUIRES
        gtest
        wifi_manager
        nvs_flash
    WHOLE_ARCHIVE
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()