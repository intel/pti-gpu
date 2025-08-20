# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import subprocess
import shutil
import argparse
import sys
import platform
import json

def load_json_config(config_file):
    try:
        with open(config_file, 'r') as file:
            data = json.load(file)
        return data
    except Exception as e:
        print(f"[ERROR] Failed to read test scenario config file: {e}")
        return None

def load_config(config_file):
    try:
        with open(config_file, 'r') as file:
            # Read all lines, strip whitespace, and filter out empty lines
            scenarios = [line.strip() for line in file.readlines() if line.strip()]
        return scenarios
    except Exception as e:
        print(f"[ERROR] Failed to read test scenario config file: {e}")
        return None

def clean_up_testing_folder(test_dir):
    # Construct the path to the 'Testing' folder
    testing_folder = os.path.join(test_dir, 'build', 'Testing')
    result_folder = os.path.join(test_dir, 'build', 'results')

    # Check if the folder exists
    if os.path.exists(testing_folder):
        shutil.rmtree(testing_folder, ignore_errors=False)
    if os.path.exists(result_folder):
        shutil.rmtree(result_folder, ignore_errors=False)

    cleanup_status = True
    # Verify if the folder was successfully removed
    if not os.path.exists(testing_folder):
        print(f"[INFO] Successfully removed testing folder : {testing_folder}")
    else:
        print(f"[ERROR] Unable to delete the testing folder '{testing_folder}'.")
        cleanup_status = False

    # Verify if the folder was successfully removed
    if not os.path.exists(result_folder):
        print(f"[INFO] Successfully removed results folder : {result_folder}")
    else:
        print(f"[ERROR] Unable to delete the results folder '{result_folder}'.")
        cleanup_status = False

    if cleanup_status == False:
        return 1
    else:
        return 0


def build(test_dir):
    # Create the build directory
    if not os.path.exists(test_dir):
        raise FileNotFoundError(f"[ERROR] Test directory does not exist: {test_dir}")

    # Set up paths
    build_dir = os.path.join(test_dir, "build")
    os.makedirs(build_dir, exist_ok = True)

    # Change to the build directory
    os.chdir(build_dir)
    try:
        subprocess.run(['cmake', '..', '-G', 'Ninja'], check = True)
        subprocess.run(['ninja'], check = True)
        return 0
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] during build: {e}")
        print(f"Return Code: {e.returncode}")
        if e.output:
            print(f"Output: {e.output.decode()}")
        if e.stderr:
            print(f"[ERROR] Output: {e.stderr.decode()}")
        return 1
    except Exception as ex:
        print(f"Unexpected [ERROR]: {ex}")
        return 1

def get_tests_in_ctest(test_dir):
    result = subprocess.run(['ctest', '--show-only=json-v1'], capture_output=True, text=True, env=os.environ, cwd=os.path.join(test_dir, 'build'))
    if result.returncode != 0:
        return []
    try:
        ctest_data = json.loads(result.stdout)
    except json.JSONDecodeError as e:
        return []
    tests = ctest_data.get("tests", [])
    uni_tests = []
    for test in tests:
        test_name = test.get("name")
        if test_name:
            uni_tests.append(test_name)
    return uni_tests

def get_filter_tests(tests_configs,default_cmd_list):
    filter_tests = {}
    for test_config in tests_configs:
        test_name = test_config.get("name")
        platforms = test_config.get("platform")
        scenarios = test_config.get("scenarios")
        skip_scenarios = test_config.get("skip_scenarios")
        enabled_scenarios = []
        if scenarios:
            for scenario in scenarios:
                if scenario == "default_scenarios":
                    enabled_scenarios.extend(default_cmd_list[:])
                else:
                    enabled_scenarios.append(scenario)
        else:
            enabled_scenarios = default_cmd_list[:]
        if skip_scenarios:
            for scenario in skip_scenarios:
                if scenario in enabled_scenarios:
                    enabled_scenarios.remove(scenario)
        filter_tests[test_name] = {"platforms":platforms, "scenarios":enabled_scenarios}
    return filter_tests

def launch_test_with_scenarios(test, scenarios, test_dir, total_tests):
    ctest_output_dir = os.path.join(test_dir, 'build', 'Testing', 'Temporary')  # Path to the CTest output directory
    failed_options = []
    sub_tests = 0
    for cmd in scenarios:
        os.environ['UNITRACE_OPTION'] = cmd
        #total_tests += 1
        sub_tests += 1
        try:
            subprocess.run(
                ['ctest', '-C', 'Release', '-R', test, '-Q'],
                check=True,
                text=True,
                env=os.environ,
                cwd=os.path.join(test_dir, 'build')
            )
            print(str(total_tests+sub_tests)+" : "+test+cmd+" : Passed")
        except subprocess.CalledProcessError as e:
            new_dir_name = test+cmd.replace('--', '_').replace('-', '_').replace(" ", "")
            failed_path = os.path.join(test_dir, 'build', 'Testing', f"Temporary_{new_dir_name}")
            failed_options.append((cmd, failed_path))
            error_message = str(total_tests+sub_tests)+" : "+test+cmd+" : Failed"
            RED = '\033[31m'
            RESET = '\033[0m' # Resets text formatting to default
            print(f"{RED}{error_message}{RESET}")

        if os.path.exists(ctest_output_dir):
            new_dir_name = test+cmd.replace('--', '_').replace('-', '_').replace(" ", "")
            new_dir_path = f"{ctest_output_dir}_{new_dir_name}"
            shutil.move(str(ctest_output_dir), str(new_dir_path))
        else:
            print(f"[ERROR] CTest output directory not found for option {cmd}. Skipping move operation.")
    return sub_tests, failed_options

def run_ctest(test_dir, scenarios):
    failed_test_flag = 0
    failed_options = []
    total_tests = 0

    # Findout default configuration
    default_config = scenarios.get("default_configuration")
    default_platforms = default_config.get("platform")
    default_scenarios = default_config.get("scenarios")

    if platform.system() not in default_platforms:
        print("[INFO] Tests are not enabled for current platform.")
        return 0

    tests = get_tests_in_ctest(test_dir)
    tests_configs = scenarios.get("test_configuration") # some tests need special handling
    filter_tests = get_filter_tests(tests_configs, default_scenarios)

    for test in tests:
        total_sub_tests = 0
        tests_status = []
        if test in filter_tests:
            if platform.system() in filter_tests[test]["platforms"]:
                total_sub_tests, tests_status = launch_test_with_scenarios(test, filter_tests[test]["scenarios"], test_dir, total_tests)
        else:
            total_sub_tests, tests_status = launch_test_with_scenarios(test, default_scenarios, test_dir, total_tests)
        total_tests += total_sub_tests
        if len(tests_status) > 0:
            for test_status in tests_status:
                failed_options.append(test_status)
    print("\n\n------------------------------------------------------------")
    print("Total test run "+str(total_tests)+ "  Failed tests : "+str(len(failed_options)))
    print("------------------------------------------------------------")
    if len(failed_options) > 0:
        print("-" * 70)
        print("\n Sub-Test Failure Report:")
        print("{:<20} {:<50} {:<30}".format("[Unitrace Option]", "[Failed Test Log Path]", "[Error Messages]"))
        print("-" * 70)

        failed_test_flag = 1
        for failed_option, path in failed_options:
            error_messages = []

            # Set the correct log file path
            log_file_path = os.path.join(path, "LastTest.log")
            if os.path.exists(log_file_path):
                try:
                    with open(log_file_path, 'r') as log_file:
                        error_block = []
                        capture_error = False

                        for line in log_file:
                            line = line.strip()
                            if '[ERROR]' in line:
                                if capture_error and error_block:
                                    # Save the previous block when a new [ERROR] is found before 'return code'
                                    error_messages.append("\n".join(error_block))
                                    error_block = []
                                capture_error = True

                            # Append lines to the current error block
                            if capture_error:
                                error_block.append(line)

                            # Stop capturing at 'return code' and save the block
                            if ('return code' in line or '<end of output>' in line) and capture_error:
                                error_messages.append("\n".join(error_block))
                                error_block = []
                                capture_error = False

                    # Handle any leftover error block
                    if capture_error and error_block:
                        error_messages.append("\n".join(error_block))

                except Exception as read_error:
                    print(f"[ERROR] reading log file {log_file_path}: {read_error}", file=sys.stderr)
            else:
                print(f"[ERROR] Log file does not exist or is empty at: {log_file_path}")

            # Format the error messages
            if error_messages:
                errors_str = "\n\n".join(error_messages)
            else:
                errors_str = "No [ERROR] found"

            # Print the error information
            print("{:<20} {:<50} {:<30}".format(failed_option, log_file_path, "\n" + errors_str))
    return failed_test_flag

def parse_arguments():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="Unitrace Test Script")

    # Optional argument for the test directory
    parser.add_argument(
        "--test-dir",
        type=str,
        default=os.getcwd(),
        help="Path to the test directory. Defaults to the current working directory."
    )
    # Flags for actions
    parser.add_argument(
        "--run",
        action="store_true",
        help="Run tests only."
    )
    parser.add_argument(
        "--config",
        type=str,
        default= os.path.join(os.getcwd(), "test_config.json"),
        help="Path to the config file. If no path is defined it will take 'test_config.json' present in the test_dir"
    )
    return parser.parse_args()

def main():
    args = parse_arguments()
    config_file = args.config

    # Check the directory and load scenarios
    if os.path.exists(config_file):
        scenarios = load_json_config(config_file)
        if (scenarios is None) or (len(scenarios) == 0):
            print("[ERROR] No scenarios found in config file")
            return 1
    else:
        print("[ERROR] No config file is present")
        return 1

    status = clean_up_testing_folder(args.test_dir)
    if status == 0:
        if args.run == False:
            status = build(args.test_dir)
            if status != 0: #build error check and return
                return status
        status = run_ctest(args.test_dir, scenarios)
    return(status)

if __name__ == "__main__":
    sys.exit(main())
