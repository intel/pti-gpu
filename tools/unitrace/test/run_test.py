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
import shutil
import glob

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
        output_filename = os.path.join(output_dir, "device-timeline_results.csv")
        device_timeline_cur.save_to_csv(device_timeline_ref, filename = output_filename)
        print(f"[INFO] Device timeline comparison complete. Result: {'Passed' if result == 0 else 'Failed'}")
        return result

    elif scenario in ["--device-timing", "-d"]:
        print(f"[INFO] Running device timing comparison for {cur_path} vs {ref_path}")
        device_timing_cur = unidiff.DeviceTiming(cur_path)
        device_timing_ref = unidiff.DeviceTiming(ref_path)
        result = device_timing_cur.compare(device_timing_ref)
        print(f"[INFO] return code : {result}")
        output_filename = os.path.join(output_dir, "device_timing_results.csv")
        device_timing_cur.save_to_csv(device_timing_ref, filename = output_filename)
        print(f"[INFO] Device timing comparison complete. Result: {'Passed' if result == 0 else 'Failed'}")
        return result

    else:
        print(f"[ERROR] Unknown scenario: {scenario}")
        return 1

def contains_non_whitespace(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return any(not ch.isspace() for ch in f.read())

def run_unitrace(cmake_root_path, scenarios, test_case_name, args, extra_test_prog, use_mpiexec=False, num_ranks=1, specific_test_case=None):
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
    test_output_name = specific_test_case if specific_test_case else os.path.basename(test_case)
    output_file = f"output{scenarios}_{test_output_name}.txt".replace("--", "_").replace("-", "_").replace(" ","")
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
    command = []
    
    # Add mpiexec if requested
    if use_mpiexec:
        command.extend(["mpiexec", "-n", str(num_ranks)])
    
    command.append(unitrace_exe)
    scenario_set = set(scenarios.split(" "))
    for scenario in scenarios.split(" "):
        command.append(scenario)

    is_chrome_logging_present = False
    if scenario.startswith("--chrome"):
        is_chrome_logging_present = True

    need_output_directory = False
    if scenario in ["--chrome-call-logging", "--chrome-device-logging","--chrome-kernel-logging", "--chrome-sycl-logging", "--chrome-itt-logging"]:
        need_output_directory = True

    if need_output_directory:
        command += ["--output-dir-path", output_dir]
    elif scenario in ["-k", "-q"]:
        command += ["-o", "metric"]
    else:
        command += ["-o", output_file_path]

    command += [test_case] + args

    print(f"[INFO] Executing command: {' '.join(command)}")

    try:
        before_list_of_files = set(os.listdir(output_dir))
        result = subprocess.run(command, text=True, capture_output=True) # executing Unitrace command
        if result.returncode != 0:
            print(f"[ERROR] Unitrace execution failed with return code {result.returncode}.", file = sys.stderr)
            return 1  # Indicate failure due to Unitrace ERROR

        # Metric logs need to be moved to result folder
        if scenario in ["-k", "-q"]:
            test_case_path = os.path.join(cmake_root_path, "build", test_case_name)
            # Find all files starting with "metric" in source directory
            pattern = os.path.join(test_case_path, "metric*")
            metric_files = glob.glob(pattern)

            # Move each file to destination
            for file_path in metric_files:
                filename = os.path.basename(file_path)
                destination_path = os.path.join(output_dir, filename)
                shutil.move(file_path, destination_path)
                os.rename(destination_path, output_dir + "/" + filename + "_" + output_file)

        after_list_of_files = set(os.listdir(output_dir))
        new_files = list(after_list_of_files - before_list_of_files)
        # check if scenario is "chrome" logging
        # then test needs to modify the file name to reflect right test case.
        if is_chrome_logging_present:
            for idx, new_file in enumerate(new_files):
                new_file = os.path.join(output_dir, new_file)
                new_file_name = ""

                new_file_name += "_" +os.path.basename(new_file)
                new_name = os.path.join(output_dir, "output") + scenarios.replace(" ","").replace("--", "_").replace("-", "_").replace(" ","") + new_file_name
                try:
                    os.rename(new_file, new_name)
                except Exception as e:
                    print(f"[ERROR] Occurred while renaming the output file: {e}", file = sys.stderr)
                    return 1
                new_files[idx] = os.path.basename(new_name)

        # Check if the output file is generated
        output_files = []
        for f in new_files:
            result_file_pattern = f.split(".")
            if (is_chrome_logging_present and "chrome" in result_file_pattern[0]
                and test_case_name.replace("/","_").split(".")[0] in result_file_pattern[0]
               ):
                output_files.append(f)
            elif scenario in ["-k", "-q"] and f.endswith(output_file):
                output_files.append(f)
            elif output_file.startswith(result_file_pattern[0]):
                output_files.append(f)
            if len(output_files) > 0 and not use_mpiexec:
                break
        if output_files:
            if use_mpiexec:
                print(f"[INFO] MPI Output files generated: {output_files}")
            else:
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
            print("[INFO] Test has custom checker, going to use it for validation.")
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
        elif not scenario_set.isdisjoint(set(["-k", "-q"])):
            min_no_of_lines = 4 # Metric files has initial few lines for headers etc..
            for output_file in output_files:
                with open(os.path.join(output_dir, output_file), 'r') as outfile:
                    # Check to make sure file has counter values generated.
                    if len(outfile.readlines()) <= min_no_of_lines:
                        print(f"[ERROR] Metric file '{output_file}' is not having counters present.")
                        return 1 # Metric file should have more than 4 line present.
        elif scenario_set.isdisjoint(set(["--device-timing", "-d", "--device-timeline", "-t"])):
            print(f"[INFO] Nothing to compare for {scenario_set} hence exiting early.")
            # check if unidiff support validation for current scenario else return early as success
            return 0
        else:
            print("[INFO] Going to run unidiff for comparison.")
            # Extract the relevant section from the expected output file
            # TODO: we need not create a buffer instead of file will work
            scenario_d_or_t = ""
            if not set(["-d","--device-timing"]).isdisjoint(scenario_set):
                scenario_d_or_t = "-d"
            elif not set(["-t","--device-timeline"]).isdisjoint(scenario_set):
                scenario_d_or_t = "-t"

            if use_mpiexec:
                # For MPI tests, the output file may have rank information
                # We will pick the latest modified file for comparison
                print(f"[INFO] MPI test detected, selecting latest output file for comparison.")
                # Parse ranks from scenarios string if --ranks-to-sample is present
                scenario_args = scenarios.split()
                device_sampled = True
                ranks_sampled = []
                if "--ranks-to-sample" in scenario_args:
                    ranks_arg_index = scenario_args.index("--ranks-to-sample")
                    if ranks_arg_index + 1 < len(scenario_args):
                        ranks_str = scenario_args[ranks_arg_index + 1]
                        ranks_sampled = [int(rank.strip()) for rank in ranks_str.split(',') if int(rank.strip()) < num_ranks]
                        print(f"[INFO] Sampled ranks detected: {ranks_sampled}")
                elif "--devices-to-sample" in scenario_args:
                    devices_arg_index = scenario_args.index("--devices-to-sample")
                    if devices_arg_index + 1 < len(scenario_args):
                        devices_str = scenario_args[devices_arg_index + 1]
                        sampled_devices = [int(device.strip()) for device in devices_str.split(',')]
                        print(f"[INFO] Sampled devices detected: {sampled_devices}")
                        if len(sampled_devices) > 1:
                            print(f"[ERROR] test don't support more then 1 device sampling for MPI tests.", file = sys.stderr)
                            return 1
                        if sampled_devices[0] != 0:
                            device_sampled = False
                        else:
                            ranks_sampled = [0]
                # Create ranks list from number of ranks
                ranks_list = []
                for rank in range(num_ranks):
                    ranks_list.append(rank)
                
                expected_output_file = extract_test_case_output(cmake_root_path, test_case_name, scenario_d_or_t)
                # Compare each sampled rank output
                all_passed = True
                for rank in ranks_list:
                    rank_output_file = None
                    for of in output_files:
                        # Extract rank from filename before .txt extension
                        if of.endswith('.txt'):
                            # Get the part before .txt
                            filename_without_ext = of[:-4]
                            # Split by '.' and get the last part which should be the rank
                            parts = filename_without_ext.split('.')
                            if parts and parts[-1].isdigit() and int(parts[-1]) == rank:
                                rank_output_file = of
                                break
                    if rank_output_file is None:
                        print(f"[ERROR] Output file for rank {rank} not found among generated files.", file = sys.stderr)
                        all_passed = False
                        continue

                    if rank in ranks_sampled and device_sampled:
                        # Call unidiff comparison
                        comparison_result = call_unidiff(scenario_d_or_t, output_dir, rank_output_file, expected_output_file)
                        if comparison_result != 0:
                            print(f"[ERROR] Unidiff comparison failed for rank {rank}.", file = sys.stderr)
                            all_passed = False
                            break
                    else:
                        # check if rank_output_file is empty file or not
                        output_file_with_path = os.path.join(output_dir, rank_output_file)
                         # check if file has any character in it (not space)
                        if contains_non_whitespace(output_file_with_path) == False:
                            print(f"[INFO] Output file for rank {rank} is empty as expected since it is not sampled.")
                        else:
                            print(f"[ERROR] Output file for rank {rank} is not empty but it was not sampled.", file = sys.stderr)
                            print("====Begin File Contents====")
                            with open(output_file_with_path, 'r') as fp:
                                contents = fp.read()
                                print(contents)
                            print("====End File Contents====")
                            all_passed = False
                            break
                if all_passed:
                    return 0
                else:
                    return 1
            else:
                expected_output_file = extract_test_case_output(cmake_root_path, test_case_name, scenario_d_or_t)

                output_file_with_pid = max(output_files, key = lambda f: os.path.getmtime(os.path.join(output_dir, f)))

                # Call unidiff comparison
                comparison_result = call_unidiff(scenario_d_or_t, output_dir, output_file_with_pid, expected_output_file)
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
    parser.add_argument('--mpiexec', action='store_true', help='Run unitrace with mpiexec')
    parser.add_argument('-n', '--num_ranks', type=int, default=1, help='Number of MPI ranks (default: 1)')
    parser.add_argument('-t','--test_case', type=str, default=None, help='Specific test case name (not the test executable name)')
    args = parser.parse_args()
    scenario = os.environ["UNITRACE_OPTION"]

    return run_unitrace(args.cmake_root_path, scenario, args.args[0], args.args[1:], 
                       args.extra_test_prog, args.mpiexec, args.num_ranks, args.test_case)

if __name__ == "__main__":
    sys.exit(main())
