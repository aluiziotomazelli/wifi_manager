# Check the build target. GTest is intended for host-based testing.
idf_build_get_property(target IDF_TARGET)
if(NOT ${target} STREQUAL "linux")
    return()
endif()

# Register this directory as an ESP-IDF component named 'gtest'.
# This allows other components to use 'REQUIRES gtest' in their CMakeLists.txt.
idf_component_register()

# CMAKE_BUILD_EARLY_EXPANSION is true during the IDF 'requirement expansion' phase.
# We wrap the FetchContent logic in this guard to ensure that the download and
# heavy processing only happen during the actual build phase, avoiding errors
# during idf.py's initial dependency discovery script mode.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    include(FetchContent)

    # Declare the external dependency: Google Test/Mock.
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # Configure GTest options before making it available.
    # We don't want to install GTest to the system, just build it locally.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

    # Download and add GTest to the build.
    # This creates targets like 'gtest' and 'gmock'.
    FetchContent_MakeAvailable(googletest)

    # Link the GTest/GMock targets to this component's library.
    # Since this component has no source files of its own, we use INTERFACE.
    target_link_libraries(${COMPONENT_LIB} INTERFACE gtest gmock)
endif()
