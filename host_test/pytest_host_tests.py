import pytest
import subprocess
import os
import re

# List of host test subdirectories
HOST_TESTS = [
    'wifi_config_storage',
    'wifi_driver_hal',
    'wifi_event_handler',
    'wifi_state_machine',
    'wifi_sync_manager',
    'integration_internal'
]

HOST_TEST_ROOT = os.path.dirname(os.path.abspath(__file__))

def build_test(test_dir):
    app_path = os.path.join(HOST_TEST_ROOT, test_dir)
    print(f"Building {test_dir}...")
    subprocess.run(["idf.py", "build"], cwd=app_path, check=True, stdout=subprocess.DEVNULL)

@pytest.mark.parametrize('test_dir', HOST_TESTS)
def test_host_component(test_dir):
    app_path = os.path.join(HOST_TEST_ROOT, test_dir)

    # 1. Build (if not already built, but idf.py is fast if no changes)
    build_test(test_dir)

    # 2. Find executable
    build_dir = os.path.join(app_path, "build")
    executable = None
    for f in os.listdir(build_dir):
        if f.endswith(".elf"):
            executable = os.path.join(build_dir, f)
            break

    assert executable is not None, f"No executable found in {build_dir}"

    # 3. Run
    process = subprocess.Popen(executable, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        stdout, _ = process.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, _ = process.communicate()
        pytest.fail(f"Test timed out for {test_dir}\nOutput:\n{stdout}")

    print(stdout)

    # 4. Check results (Unity output)
    summary_match = re.search(r"(\d+) Tests (\d+) Failures (\d+) Ignored", stdout)
    assert summary_match is not None, f"Unity test summary not found in output for {test_dir}"

    failures = int(summary_match.group(2))
    assert failures == 0, f"Test failed with {failures} failures in {test_dir}\nOutput:\n{stdout}"
    assert "OK" in stdout, f"Unity test final status was not OK in {test_dir}\nOutput:\n{stdout}"
