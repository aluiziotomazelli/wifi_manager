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

For running all tests at once and generating a single unified coverage report for the `wifi_manager` component:

1. **Configure the project**:
   ```bash
   cd host_test
   mkdir -p build && cd build
   cmake ..
   ```

2. **Build all test projects**:
   ```bash
   make build_all_tests
   ```

3. **Clean old coverage data and run tests**:
   ```bash
   make coverage_clean
   ctest --output-on-failure
   ```

4. **Generate the unified coverage report**:
   ```bash
   make unified_coverage
   ```
   The report will be available at `host_test/coverage/index.html`.

## CI/CD Integration

This project includes a GitHub Actions workflow (`.github/workflows/host_test.yml`) that automatically:
- Installs necessary dependencies (`lcov`).
- Builds and runs all host tests on every push/PR to `main`.
- Generates the unified coverage report.
- Deploys the coverage report to GitHub Pages (only on pushes to `main`).

## Shared Coverage Logic

The coverage logic is centralized in `host_test/coverage_common.cmake`. Individual test projects include this file to maintain consistency and reduce duplication.