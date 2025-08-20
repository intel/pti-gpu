# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import sys
import subprocess
import os
import platform
import unidiff
import argparse

def extract_test_case_output(cmake_root_path, test_case_name, scenario):
    # Construct the path to the expected output file within the "gold" folder
    scenario = scenario.lstrip('-')
    if platform.system() == "Windows":
        expected_output_file = os.path.join(cmake_root_path, test_case_name, "gold", "windows", f"{scenario}.txt").replace("\\", "/")
    else:
        expected_output_file = os.path.join(cmake_root_path, test_case_name, "gold", "linux", f"{scenario}.txt").replace("\\", "/")

    print(f"[INFO] Extracting expected output from file: {expected_output_file}")

    # Check if the file exists
    if not os.path.isfile(expected_output_file):
        raise FileNotFoundError(f"[ERROR] Expected output file {expected_output_file} not found")

    # Return the path to the expected output file
    return expected_output_file

def call_unidiff(scenario, output_dir, output_file_with_pid, expected_output_file):
    cur_path = os.path.join(output_dir, output_file_with_pid)
    ref_path = expected_output_file

    if scenario in ["--device-timeline", "-t"]:
        print(f"[INFO] Running device timeline comparison for {cur_path} vs {ref_path}")
        device_timeline_cur = unidiff.DeviceTimeline(cur_path)
        device_timeline_ref = unidiff.DeviceTimeline(ref_path)
        result = device_timeline_cur.compare(device_timeline_ref)
        print(f"[INFO] return code : {result}")
        device_timeline_cur.save_to_csv(device_timeline_ref, filename="device-timeline_results.csv")
        print(f"[INFO] Device timeline comparison complete. Result: {'Passed' if result == 0 else 'Failed'}")
        return result

    elif scenario in ["--device-timing", "-d"]:
        print(f"[INFO] Running device timing comparison for {cur_path} vs {ref_path}")
        device_timing_cur = unidiff.DeviceTiming(cur_path)
        device_timing_ref = unidiff.DeviceTiming(ref_path)
        result = device_timing_cur.compare(device_timing_ref)
        print(f"[INFO] return code : {result}")
        device_timing_cur.save_to_csv(device_timing_ref, filename="device_timing_results.csv")
        print(f"[INFO] Device timing comparison complete. Result: {'Passed' if result == 0 else 'Failed'}")
        return result

    else:
        print(f"[ERROR] Unknown scenario: {scenario}")
        return 1

def run_unitrace(cmake_root_path, scenarios, test_case_name, args, extra_test_prog):
    output_dir = cmake_root_path + "/build/results"
    if platform.system() == "Windows":
        unitrace_exe = cmake_root_path + "/../build/unitrace.exe"
        test_case = os.path.join(cmake_root_path, "build", test_case_name, f"{test_case_name}.exe").replace("\\", "/")

    else:
        unitrace_exe = cmake_root_path + "/../build/unitrace"
        test_case = os.path.join(cmake_root_path, "build", test_case_name, f"{test_case_name}").replace("\\", "/")

    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok = True)

    # Generate a unique output file name
    output_file = f"output{scenarios}_{os.path.basename(test_case)}.txt".replace("--", "_").replace("-", "_").replace(" ","")
    output_file_path = os.path.join(output_dir, output_file).replace("\\", "/")

    # Check if Unitrace executable exists
    if not os.path.exists(unitrace_exe):
        print(f"[ERROR] Unitrace executable not found at {unitrace_exe}", file=sys.stderr)
        return 1

    # Check if test case executable exists
    if not os.path.exists(test_case):
        print(f"[ERROR] Test case executable not found at {test_case}", file=sys.stderr)
        return 1

    # Prepare command here
    command = [unitrace_exe]
    for scenario in scenarios.split(" "):
        command.append(scenario)
    command += ["-o", output_file_path, test_case] + args

    print(f"[INFO] Executing command: {' '.join(command)}")

    try:
        result = subprocess.run(command, text=True, capture_output=True) # executing Unitrace command
        if result.returncode != 0:
            print(f"[ERROR] Unitrace execution failed with return code {result.returncode}.", file = sys.stderr)
            return 1  # Indicate failure due to Unitrace ERROR

        # Check if the output file is generated
        output_files = []
        for f in os.listdir(output_dir):
            result_file_pattern = f.split(".")
            if output_file.startswith(result_file_pattern[0]):
                output_files.append(f)
                break
        if output_files:
            print(f"[INFO] Output file '{output_files[0]}' generated successfully.")
        else:
            print(f"[ERROR] Output file matching pattern '{output_file}' not found.", file = sys.stderr)
            return 1  # Returning non-zero indicates failure

        # Validating the test output
        # Check if the output file contains '[ERROR]'
        # TODO: we will keep output in the buffer
        with open(os.path.join(output_dir, output_files[0]), "r") as outfile:
            output_content = outfile.read()
            if "[ERROR]" in output_content:
                print(f"[ERROR] found in output file '{output_file}'.", file = sys.stderr)
                return 1  # Indicate failure due to ERROR in output

        if extra_test_prog != None:
            # obtain list of generated files from unitrace stderr output
            full_path_output_files = []
            err_lines = result.stderr.splitlines()
            for line in err_lines:
                i = line.find("is stored in")
                if i > 0:
                    full_path_output_files.append(line[i+13:].strip())

            # execute provided test with list of files as arguments
            if extra_test_prog.endswith(".py"):
                cmd = [sys.executable, extra_test_prog] + full_path_output_files
            else:
                cmd = [extra_test_prog] + full_path_output_files

            rc = subprocess.run(cmd, text=True)
            if rc.returncode != 0:
                return 1  # extra test prog failed
            else:
                return 0
        elif scenarios not in ["--device-timing", "-d", "--device-timeline", "-t"]:
            # check if unidiff support validation for current scenario else return early as success
            return 0
        else:
            # Extract the relevant section from the expected output file
            # TODO: we need not create a buffer instead of file will work
            expected_output_file = extract_test_case_output(cmake_root_path, test_case_name, scenarios)

            output_file_with_pid = max(output_files, key = lambda f: os.path.getmtime(os.path.join(output_dir, f)))

            # Call unidiff comparison
            comparison_result = call_unidiff(scenarios, output_dir, output_file_with_pid, expected_output_file)
            if comparison_result != 0:
                print(f"[ERROR] Unidiff comparison failed.", file = sys.stderr)
                return 1
            else:
                return 0
    except Exception as e:
          print(f"[ERROR] Occurred while running unitrace: {e}", file = sys.stderr)
          return 1

def main():
    parser = argparse.ArgumentParser(prog="run_test.py", description="Run unitrace test")
    parser.add_argument('-e', '--extra_test_prog', type=str)
    parser.add_argument('cmake_root_path')
    parser.add_argument('args', nargs='+')
    args = parser.parse_args()

    scenario = os.environ["UNITRACE_OPTION"]

    return run_unitrace(args.cmake_root_path, scenario, args.args[0], args.args[1:], args.extra_test_prog)

if __name__ == "__main__":
    sys.exit(main())
