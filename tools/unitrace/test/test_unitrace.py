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

    # Check if the folder exists
    if os.path.exists(testing_folder):
        shutil.rmtree(testing_folder, ignore_errors=False)

    # Verify if the folder was successfully removed
    if not os.path.exists(testing_folder):
        print(f"[INFO] Successfully removed testing folder : {testing_folder}")
        return 0
    else:
        print(f"[ERROR] Unable to delete the folder '{testing_folder}'.")
        return 1

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

def run_ctest(test_dir, scenarios):
    ctest_output_dir = os.path.join(test_dir, 'build', 'Testing', 'Temporary')  # Path to the CTest output directory

    # Loop through the unitrace options
    failed_test_flag = 0
    failed_options = []

    for option in scenarios:
        os.environ['UNITRACE_OPTION'] = option
        print(f" [INFO] Testing scenario {option}")
        
        # Launch CTest with the environment variable set
        try:
            if platform.system() == 'Linux' and option == '-t':
                print("[INFO] Skipping 'graph' test for option '-t'")
                subprocess.run(
                    ['ctest', '-C', 'Release', '-E', 'graph'],  # Exclude the 'graph' test case
                    check=True,
                    text=True,
                    env=os.environ,
                    cwd=os.path.join(test_dir, 'build')
              )
            else:
                subprocess.run(
                    ['ctest', '-C', 'Release'],
                    check=True,
                    text=True,
                    env=os.environ,
                    cwd=os.path.join(test_dir, 'build')
                ) 

        except subprocess.CalledProcessError as e:
            new_dir_name = option.replace('--', '').replace('-', '')
            failed_path = os.path.join(test_dir, 'build', 'Testing', f"Temporary_{new_dir_name}")
            failed_test_flag = 1
            failed_options.append((option, failed_path))
            print(f"[ERROR] CTest failed with option {option}: {e}", file=sys.stderr)

        if os.path.exists(ctest_output_dir):
            new_dir_name = option.replace('--', '').replace('-', '')
            new_dir_path = f"{ctest_output_dir}_{new_dir_name}"
            print(f"[INFO] Renaming {ctest_output_dir} to {new_dir_path}")
            shutil.move(str(ctest_output_dir), str(new_dir_path))
        else:
            print(f"[ERROR] CTest output directory not found for option {option}. Skipping move operation.")

    if failed_options:
        print("-" * 70)
        print("\n Sub-Test Failure Report:")
        print("{:<20} {:<50} {:<30}".format("[Unitrace Option]", "[Failed Test Log Path]", "[Error Messages]"))
        print("-" * 70)

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
        default= os.path.join(os.getcwd(), "scenarios.txt"),
        help="Path to the config file. If no path is defined it will take 'scenarios.txt' present in the test_dir"
    )
    return parser.parse_args()

def main():
    args = parse_arguments()
    config_file = args.config

    # Check the directory and load scenarios
    if os.path.exists(config_file):
        scenarios = load_config(config_file)
        if (scenarios is None) or (len(scenarios) == 0):
            print("[ERROR] No scenarios found in config file")
            return 1
    else:
        print("[ERROR] No config file is present")
        return 1

    if args.run:
        status = clean_up_testing_folder(args.test_dir)
        if status == 0:
            status = run_ctest(args.test_dir, scenarios)
    else:
        status = clean_up_testing_folder(args.test_dir)
        if status == 0:
            status = build(args.test_dir)
            if status == 0:
                status = run_ctest(args.test_dir, scenarios)

    return(status)

if __name__ == "__main__":
    sys.exit(main())
