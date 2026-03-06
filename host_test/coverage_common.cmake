# Common coverage logic for host-based tests
# This file is included by individual test projects to avoid duplication.

macro(setup_gtest_coverage PROJECT_NAME EXECUTABLE_NAME)
    if(IDF_TARGET STREQUAL "linux")
        find_program(LCOV_PATH lcov REQUIRED)
        find_program(GENHTML_PATH genhtml REQUIRED)

        # Each test project will have its own coverage directory
        set(COVERAGE_DIR "${PROJECT_SOURCE_DIR}/coverage")

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
            # COMMAND ${LCOV_PATH} --extract coverage.info "*/wifi_manager/src/*" "*/wifi_manager/include/*" --output-file coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
            COMMAND ${LCOV_PATH} --extract coverage.info "*/wifi_manager/src/*" --output-file coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
            COMMAND ${LCOV_PATH} --remove coverage_filtered.info "*/wifi_manager/include/interfaces/*" --output-file coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
            COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory ${COVERAGE_DIR} --title "${PROJECT_NAME} Component Coverage" --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused
            COMMAND ${LCOV_PATH} --list coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating filtered coverage report for ${PROJECT_NAME}"
        )
    endif()
endmacro()
