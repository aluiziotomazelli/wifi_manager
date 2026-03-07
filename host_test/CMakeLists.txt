cmake_minimum_required(VERSION 3.16)
project(wifi_manager_host_tests NONE)

# Enable CTest for this project
include(CTest)

# Define the tests for CTest.
# These commands assume that each project has been built using 'idf.py build'
# and that CTest is being run from a 'build' subdirectory within 'host_test'.
add_test(NAME test_bootstrapper COMMAND ../test_bootstrapper/build/test_bootstrapper.elf)
add_test(NAME test_config_storage COMMAND ../test_config_storage/build/test_config_storage.elf)
add_test(NAME test_event_handler COMMAND ../test_event_handler/build/test_event_handler.elf)
add_test(NAME test_message_processor COMMAND ../test_message_processor/build/test_message_processor.elf)
add_test(NAME test_state_machine COMMAND ../test_state_machine/build/test_state_machine.elf)
add_test(NAME test_sync_manager COMMAND ../test_sync_manager/build/test_sync_manager.elf)
add_test(NAME test_wifi_manager COMMAND ../test_wifi_manager/build/test_wifi_manager.elf)

# Unified Coverage Configuration
find_program(LCOV_PATH lcov REQUIRED)
find_program(GENHTML_PATH genhtml REQUIRED)

# The unified coverage report will be placed in host_test/coverage
set(UNIFIED_COVERAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/coverage")

# This target aggregates coverage data (.gcda files) from all test build directories
add_custom_target(unified_coverage
    COMMAND ${LCOV_PATH} --capture --directory .. --output-file unified_coverage.info --rc branch_coverage=1 --rc geninfo_unexecuted_blocks=1 --ignore-errors mismatch,inconsistent
    # COMMAND ${LCOV_PATH} --extract unified_coverage.info "*/wifi_manager/src/*" "*/wifi_manager/include/*" --output-file unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    COMMAND ${LCOV_PATH} --extract unified_coverage.info "*/wifi_manager/src/*" --output-file unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    COMMAND ${LCOV_PATH} --remove unified_coverage_filtered.info "*/wifi_manager/include/interfaces/*" --output-file unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    COMMAND ${GENHTML_PATH} unified_coverage_filtered.info --output-directory ${UNIFIED_COVERAGE_DIR} --title "WiFi Manager Unified Coverage" --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused
    COMMAND ${LCOV_PATH} --list unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating unified coverage report from all host tests"
)

# Added cleanup target to manually clear previous results if needed
add_custom_target(coverage_clean
    COMMAND find . -name "*.gcda" -delete
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${UNIFIED_COVERAGE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Cleaning up coverage data"
)

# Helper target to build all tests at once (requires idf.py to be in your PATH)
add_custom_target(build_all_tests
    COMMAND idf.py -C ../test_bootstrapper build
    COMMAND idf.py -C ../test_config_storage build
    COMMAND idf.py -C ../test_event_handler build
    COMMAND idf.py -C ../test_message_processor build
    COMMAND idf.py -C ../test_state_machine build
    COMMAND idf.py -C ../test_sync_manager build
    COMMAND idf.py -C ../test_wifi_manager build
    COMMENT "Building all test projects using idf.py"
)
