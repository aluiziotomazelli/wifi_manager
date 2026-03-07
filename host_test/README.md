# WiFi Manager Host Tests

This directory contains host-based tests for the component. Host testing allows you to run unit tests on your development machine (Linux) instead of the target microcontroller, enabling faster development cycles and the use of advanced mocking frameworks like Google Mock.

## How to Run Tests

### Prerequisites

- A Linux host machine.
- ESP-IDF environment set up and sourced.
- libbsd and libbsd-dev packages for host tests on linux target.
- lcov and genhtml installed for coverage reporting.
- The project root directory must be named `wifi_manager`. 
  - Idf build system uses "Preset Component Variables" that are available for use, but should not be modified: 
  `COMPONENT_NAME`: Name of the component. Same as the name of the component directory.


### Running tests

The tests are placed in a directories for each subclass of the component. To run a specific test, navigate to the test directory and run the following commands:

1. Navigate to the test project directory:
   ```bash
   cd host_test/test_bootstrapper
   ```

2. Set the target to Linux:
   ```bash
   idf.py --preview set-target linux
   ```

3. Build the test:
   ```bash
   idf.py build
   ```

4. Run the executable:
   ```bash
   ./build/test_bootstrapper.elf
   ```

### Generating Coverage

After running tests in the test directory, you can generate a coverage report for that test by running the following command:

```bash
idf.py coverage
```
This will generate a coverage report in the `coverage` directory of the test project, e.g. `host_test/test_bootstrapper/coverage`.


## Unified Testing and Coverage (all tests)

For production readiness or CI, you can run all tests and generate a single unified coverage report.

1. Configure and build all tests:
   ```bash
   cd host_test
   cmake -B build -S .
   cmake --build build --target build_all_tests
   ```
2. Run all tests:
   ```bash
   # from the host_test directory 
   ctest --build-dir build/
   
   # or from the build directory
   cd host_test/build
   ctest
   ```
3. Generate unified coverage report:
   ```bash
   # From the host_test directory
   cmake --build build --target unified_coverage
   ```

## Shared Coverage Logic

The coverage logic is centralized in `host_test/coverage_common.cmake`. Individual test projects include this file to maintain consistency and reduce duplication.