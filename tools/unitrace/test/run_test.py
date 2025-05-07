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

def extract_test_case_output(cmake_root_path, test_case_nmae, scenario):
    # Construct the path to the expected output file within the "gold" folder
    scenario = scenario.lstrip('-')
    if platform.system() == "Windows":
        expected_output_file = os.path.join(cmake_root_path, test_case_nmae, "gold", "windows", f"{scenario}.txt").replace("\\", "/")
    else:
        expected_output_file = os.path.join(cmake_root_path, test_case_nmae, "gold", "linux", f"{scenario}.txt").replace("\\", "/")

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

def run_unitrace(cmake_root_path, scenario, test_case_nmae, args):
    output_dir = cmake_root_path + "/build/results"
    if platform.system() == "Windows":
        unitrace_exe = cmake_root_path + "/../build/unitrace.exe"
        test_case = os.path.join(cmake_root_path, "build", test_case_nmae, f"{test_case_nmae}.exe").replace("\\", "/")

    else:
        unitrace_exe = cmake_root_path + "/../build/unitrace"
        test_case = os.path.join(cmake_root_path, "build", test_case_nmae, f"{test_case_nmae}").replace("\\", "/")

    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok = True)

    # Generate a unique output file name
    output_file = f"output_{scenario}_{os.path.basename(test_case)}.txt".replace("--", "_").replace("-", "_")
    output_file_path = os.path.join(output_dir, output_file).replace("\\", "/")

    # Check if unitrace executable exists
    if not os.path.exists(unitrace_exe):
        print(f"[ERROR] Unitrace executable not found at {unitrace_exe}", file=sys.stderr)
        return 1

    # Check if test case executable exists
    if not os.path.exists(test_case):
        print(f"[ERROR] Test case executable not found at {test_case}", file=sys.stderr)
        return 1

    command = [unitrace_exe, "--opencl", scenario, "-o", output_file_path, test_case] + args

    print(f"[INFO] Executing command: {' '.join(command)}")

    try:

        result = subprocess.run(command, text=True) # executing unitrace command
        if result.returncode != 0:
            print(f"[ERROR] Unitrace execution failed with return code {result.returncode}.", file = sys.stderr)
            return 1  # Indicate failure due to unitrace ERROR

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

        # check for unidiff supported commands
        if scenario not in ["--device-timing", "-d", "--device-timeline", "-t"]:
            return 0 # unidiff.py not required for this scenario

        # Extract the relevant section from the expected output file
        # TODO: we need not create a buffer instead of file will work
        expected_output_file = extract_test_case_output(cmake_root_path, test_case_nmae, scenario)

        output_file_with_pid = max(output_files, key = lambda f: os.path.getmtime(os.path.join(output_dir, f)))

        # Call unidiff comparison
        comparison_result = call_unidiff(scenario, output_dir, output_file_with_pid, expected_output_file)
        if comparison_result != 0:
            print(f"[ERROR] Unidiff comparison failed.", file = sys.stderr)
            return 1

    except Exception as e:
          print(f"[ERROR] Occurred while running unitrace: {e}", file = sys.stderr)
          return 1

def main():
    if len(sys.argv) < 3:
        print("[ERROR] Minimum 4 arguments required")
        print(sys.argv)
        return 1

    cmake_root_path = sys.argv[1]
    scenario = os.environ["UNITRACE_OPTION"]
    test_case_name = sys.argv[2]
    args = sys.argv[3:]

    return run_unitrace(cmake_root_path, scenario, test_case_name, args)
    
if __name__ == "__main__":
    sys.exit(main())
