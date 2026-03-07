# Common coverage logic for host-based tests
# This file is included by individual test projects to avoid duplication.

macro(setup_gtest_coverage PROJECT_NAME EXECUTABLE_NAME)
    if(IDF_TARGET STREQUAL "linux")
        find_program(LCOV_PATH lcov REQUIRED)
        find_program(GENHTML_PATH genhtml REQUIRED)

        # Each test project will have its own coverage directory
        set(COVERAGE_DIR "${PROJECT_SOURCE_DIR}/coverage")

        # Set default patterns to extract coverage
        if(NOT EXTRACT_PATTERNS_${PROJECT_NAME})
            set(EXTRACT_PATTERNS_${PROJECT_NAME} "*/wifi_manager/src/*" "*/wifi_manager/include/*")
        endif()

        # Set default patterns to remove
        if(NOT REMOVE_PATTERNS_${PROJECT_NAME})
            set(REMOVE_PATTERNS_${PROJECT_NAME} "*/wifi_manager/include/interfaces/*")
        endif()

        # Extraction command for coverage
        set(EXTRACT_COMMAND ${LCOV_PATH} --extract coverage.info)
        foreach(pattern ${EXTRACT_PATTERNS_${PROJECT_NAME}})
            list(APPEND EXTRACT_COMMAND "${pattern}")
        endforeach()
        list(APPEND EXTRACT_COMMAND --output-file coverage_temp.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty)

        # Removal command for coverage
        set(REMOVE_COMMAND ${LCOV_PATH} --remove coverage_temp.info)
        foreach(pattern ${REMOVE_PATTERNS_${PROJECT_NAME}})
            list(APPEND REMOVE_COMMAND "${pattern}")
        endforeach()
        list(APPEND REMOVE_COMMAND --output-file coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty)        

        # Custom target to run tests and generate a filtered coverage report
        # This target:
        # 1. Cleans old coverage data (.gcda files)
        # 2. Runs the test executable
        # 3. Captures coverage data using lcov
        # 4. Filters data to include only the component's source and include directories
        # 5. Generates an HTML report
        add_custom_target(generate_coverage
            COMMAND find . -name "*.gcda" -delete
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${COVERAGE_DIR}
            COMMAND ./${EXECUTABLE_NAME}
            COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage.info --rc branch_coverage=1 --rc geninfo_unexecuted_blocks=1 --ignore-errors mismatch,inconsistent
            COMMAND ${EXTRACT_COMMAND}
            COMMAND ${REMOVE_COMMAND}
            COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory ${COVERAGE_DIR} --title "${PROJECT_NAME} Coverage" --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused
            COMMAND ${LCOV_PATH} --list coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating filtered coverage report for ${PROJECT_NAME}"
        )
    endif()
endmacro()