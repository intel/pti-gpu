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
from pathlib import Path
from datetime import datetime

import test_layout

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
        print(f"[INFO] Running device timeline comparison for {cur_path} vs {ref_path}")
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
        print(f"[INFO] Running device timing comparison for {cur_path} vs {ref_path}")
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

def run_unitrace(cmake_root_path, scenarios, test_case_name, args, extra_test_prog, test_bin_dir, test_output_dir, use_mpiexec=False, num_ranks=1, specific_test_case=None, unitrace_exe_override=None, skip_golden_compare=False, results_subdir=None):
    is_python_test = test_case_name.find("python") != -1
    unitrace_exe = cmake_root_path + "/../../build/" + test_layout.exe("unitrace")
    test_case = test_layout.workload_exe(test_bin_dir, test_case_name).replace("\\", "/")

    if is_python_test:
        test_case = test_case_name

    # Explicit launcher location, for a unitrace built or delivered outside
    # the standalone tools/unitrace layout.
    if unitrace_exe_override:
        unitrace_exe = unitrace_exe_override
    
    # Check if test case executable exists
    if not os.path.exists(test_case):
        print(f"[ERROR] Test case executable not found at {test_case}", file=sys.stderr)
        return 1

    # "native" scenario: run the application by itself, without unitrace. This is
    # a sanity check for the driver/environment. If the app cannot even run on
    # its own, the problem is not unitrace, so the caller can skip the remaining
    # unitrace scenarios for this test. This runs before the unitrace executable
    # check on purpose, so a missing/bad unitrace build cannot mask a driver issue.
    if scenarios.strip() == "native":
        native_command = []
        if use_mpiexec:
            native_command.extend(["mpiexec", "-n", str(num_ranks)])
        native_command += [test_case] + args
        print(f"[INFO] Native run (app only, no unitrace): {' '.join(native_command)}")
        try:
            native_result = subprocess.run(native_command, text=True, capture_output=True)
        except Exception as e:
            print(f"[ERROR] DRIVER/ENVIRONMENT ISSUE (NOT a unitrace problem): could not "
                  f"launch the application '{' '.join(native_command)}': {e}", file = sys.stderr)
            return 1
        if native_result.returncode != 0:
            print(f"[ERROR] DRIVER/ENVIRONMENT ISSUE (NOT a unitrace problem): the "
                  f"application failed to run on its own with return code "
                  f"{native_result.returncode}, before unitrace was involved.", file = sys.stderr)
            print(f"[ERROR] Native command '{' '.join(native_command)}' failed.", file = sys.stderr)
            print(f"[ERROR] Native stderr output: {native_result.stderr}", file = sys.stderr)
            print(f"[ERROR] Native stdout output: {native_result.stdout}", file = sys.stderr)
            return 1
        print(f"[INFO] Native run succeeded.", file = sys.stderr)
        return 0

    # Check if Unitrace executable exists (unitrace scenarios only)
    if not os.path.exists(unitrace_exe):
        print(f"[ERROR] Unitrace executable not found at {unitrace_exe}", file=sys.stderr)
        return 1

    scenario_set = set(scenarios.split(" "))
    is_result_dir = "--result-dir" in scenario_set
    # Prepare command here
    command = []
    
    # Add mpiexec if requested
    if use_mpiexec:
        command.extend(["mpiexec", "-n", str(num_ranks)])
    
    command.append(unitrace_exe)
    
    for scenario in scenarios.split(" "):
        if scenario != "--result-dir":
            command.append(scenario)

    test_output_name = specific_test_case if specific_test_case else os.path.basename(test_case) #graph
    output_dir = test_layout.results_dir(test_output_dir, results_subdir).replace("\\", "/")
    
    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok = True)

    if is_result_dir:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        if platform.system() == "Windows":
            # Windows has limit to MAX_PATH (260 characters)
            output_result_dir = timestamp
        else:
            output_result_dir = f"output{scenarios}_{test_output_name}_{timestamp}".replace("--", "_").replace("-", "_").replace(" ","")
        output_result_dir_path = os.path.join(output_dir, output_result_dir).replace("\\", "/")
        os.makedirs(output_result_dir_path, exist_ok = True)
    else:
        output_file = f"output{scenarios}_{test_output_name}.txt".replace("--", "_").replace("-", "_").replace(" ","") #output_k_result_dir_graph.txt
        output_file_path = os.path.join(output_dir, output_file).replace("\\", "/")
        
    metric_dir = os.path.join(test_layout.metric_scratch_dir(test_output_dir, results_subdir),
                              specific_test_case if is_python_test and specific_test_case is not None
                              else test_case_name)

    # Dispatch on the scenario's token set, not on any single token's position.
    is_chrome_logging_present = any(token.startswith("--chrome") for token in scenario_set)
    is_metrics_scenario = not scenario_set.isdisjoint({"-k", "-q"})
    if is_result_dir:
        command += ["--result-dir", output_result_dir_path]
    else:
        if is_chrome_logging_present:
            command += ["--output-dir-path", output_dir]
        elif is_metrics_scenario:
            # Metric files are written next to the -o base name; anchor it at
            # a per-test scratch directory (where the collection glob below
            # looks) so the run does not depend on the working directory.
            os.makedirs(metric_dir, exist_ok = True)
            command += ["-o", os.path.join(metric_dir, "metric")]
        else:
            command += ["-o", output_file_path]

    command += [test_case] + args

    # unitrace reports an over-long Windows path only as a failure to open the
    # file, so name the real cause up front. Estimate the longest path it
    # writes: <result dir>/<app>.<pid>/<longest report name>, or <-o file>
    # with .<pid> inserted.
    if is_result_dir:
        app_name = os.path.splitext(os.path.basename(test_case))[0]
        longest_path = f"{output_result_dir_path}/{app_name}.000000/ccl_summary_report.txt"
    elif not is_chrome_logging_present and not is_metrics_scenario:
        longest_path = output_file_path[:-len(".txt")] + ".000000.txt"
    else:
        longest_path = ""
    if test_layout.windows_path_too_long(longest_path):
        print(f"[WARNING] unitrace output paths may reach {len(longest_path)} characters, "
              f"over the Windows MAX_PATH limit ({test_layout.WINDOWS_MAX_PATH}): {longest_path}. "
              f"Pass a shorter --output-dir to test_unitrace.py.", file = sys.stderr)

    print(f"[INFO] Executing command: {' '.join(command)}")

    try:
        before_list_of_files = set(os.listdir(output_dir))
        result = subprocess.run(command, text=True, capture_output=True) # executing Unitrace command
        if result.returncode != 0:
            print(f"[ERROR] Unitrace execution failed with return code {result.returncode}.", file = sys.stderr)
            print(f"[ERROR] Unitrace command {' '.join(command)} failed.", file = sys.stderr)
            print(f"[ERROR] Unitrace stderr output: {result.stderr}", file = sys.stderr)
            print(f"[ERROR] Unitrace stdout output: {result.stdout}", file = sys.stderr)
            return 1  # Indicate failure due to Unitrace ERROR

        print(f"[ERROR] Unitrace stderr output: {result.stderr}", file = sys.stderr)
        print(f"[ERROR] Unitrace stdout output: {result.stdout}", file = sys.stderr)
        
        if not is_result_dir:
            # Metric logs need to be moved to result folder
            if is_metrics_scenario:
                # Find all files starting with "metric" in the scratch directory
                pattern = os.path.join(metric_dir, "metric*")
                metric_files = glob.glob(pattern)
                print(f"[INFO] Found metric files: {metric_files}", file = sys.stderr)
                # Move each file to destination
                for file_path in metric_files:
                    filename = os.path.basename(file_path)
                    destination_path = os.path.join(output_dir, filename)
                    shutil.move(file_path, destination_path)
                    os.rename(destination_path, output_dir + "/" + filename + "_" + output_file)
                    print(f"[INFO] Moved metric file {file_path} to {destination_path}", file = sys.stderr)

            after_list_of_files = set(os.listdir(output_dir))
            new_files = list(after_list_of_files - before_list_of_files)
            # check if scenario is "chrome" logging
            # then test needs to modify the file name to reflect right test case.
            if is_chrome_logging_present:
                min_no_of_lines = 4 # Metric files has initial few lines for headers etc..
                print(f"[INFO] Chrome logging scenario detected, renaming output files to reflect test case name.", file = sys.stderr)
                for idx, new_file in enumerate(new_files):
                    new_file = os.path.join(output_dir, new_file)
                    new_file_name = ""
                    if new_file.find("python") != -1 and new_file.find(".json") != -1:
                        new_file_name = "_" + specific_test_case + ".json"
                    else:
                        new_file_name += "_" +os.path.basename(new_file)
                    new_name = os.path.join(output_dir, "output") + scenarios.replace(" ","").replace("--", "_").replace("-", "_").replace(" ","") + new_file_name
                    try:
                        os.rename(new_file, new_name)
                    except Exception as e:
                        print(f"[ERROR] Occurred while renaming the output file: {e}", file = sys.stderr)
                        return 1
                    new_files[idx] = os.path.basename(new_name)
                    print(f"[INFO] Renamed chrome logging output file to {new_name}", file = sys.stderr)

        # Check if the output file is generated
        output_files = []
        if is_result_dir:
            result_dir = Path(output_result_dir_path)
            all_files = [str(f.relative_to(result_dir)) for f in result_dir.rglob("*") if f.is_file()]
            for file in all_files:
                print(f"[INFO] Found file: {file}")
            
            if is_chrome_logging_present:
                output_files = [str(f.relative_to(result_dir)) for f in result_dir.rglob("chrome_trace.json")]
                print(f"[INFO] Found chrome_trace.json files: {output_files}")
            elif "-k" in scenario_set or "-q" in scenario_set:
                # The metric data is the primary artifact of -k/-q. The file is
                # metrics_<device>.csv when written by the in-process collector
                # and metrics.csv when the parent computes it from raw data
                # (Windows metric query).
                output_files = [str(f.relative_to(result_dir))
                                for pattern in ("metrics_*", "metrics.csv")
                                for f in result_dir.rglob(pattern) if f.is_file()]
                print(f"[INFO] Found metrics files: {output_files}")
                # -k/-q also imply device timing when no chrome logging is
                # requested, so require the timing report too: a missing report
                # (e.g. lost during process teardown) must fail the test.
                timing_files = [str(f.relative_to(result_dir))
                                for f in result_dir.rglob("device_timing.txt") if f.is_file()]
                print(f"[INFO] Found timing files: {timing_files}")
                if not timing_files:
                    print(f"[ERROR] device_timing.txt not found.", file = sys.stderr)
                    output_files = []
            else:
                file_patterns = [
                    "host_timing.txt",
                    "device_timing.txt", 
                    "device_submission.txt",
                    "trace.txt",
                    "call_logging.txt",
                    "device_timeline.txt",
                    "ccl_summary_report.txt"
                ]
                output_files = [str(f.relative_to(result_dir)) 
                                for pattern in file_patterns 
                                for f in result_dir.rglob(pattern)]
                print(f"[INFO] Found timing/trace files: {output_files}")
        else:
            for f in new_files:
                result_file_pattern = f.split(".")
                if (is_chrome_logging_present and "chrome" in result_file_pattern[0]
                    and (test_case_name.replace("/","_").split(".")[0] in result_file_pattern[0] or (specific_test_case is not None and specific_test_case in result_file_pattern[0]))
                ):
                    output_files.append(f)
                elif is_metrics_scenario and f.endswith(output_file) and "metrics" in f:
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
            if not use_mpiexec:
                if is_result_dir:
                    print(f"[ERROR] Output file not found.", file = sys.stderr)
                else:
                    print(f"[ERROR] Output file matching pattern '{output_file}' not found.", file = sys.stderr)
                return 1  # Returning non-zero indicates failure
            return 0

        # Validating the test output
        # Check if the output file contains '[ERROR]'
        # TODO: we will keep output in the buffer
        if is_result_dir:
            output_file_full_path = os.path.join(output_result_dir_path, output_files[0])
        else:
            output_file_full_path = os.path.join(output_dir, output_files[0])
        with open(output_file_full_path, "r") as outfile:
            output_content = outfile.read()
            if "[ERROR]" in output_content:
                print(f"[ERROR] found in output file '{output_file_full_path}'.", file = sys.stderr)
                return 1  # Indicate failure due to ERROR in output

        if extra_test_prog != None:
            print("[INFO] Test has custom checker, going to use it for validation.", file = sys.stderr)
            # obtain list of generated files from unitrace stderr output
            full_path_output_files = []
            if is_result_dir:
                full_path_output_files = [str(f) for f in result_dir.rglob("*") if f.is_file()]
            else:
                err_lines = result.stderr.splitlines()
                for line in err_lines:
                    i = line.find("is stored in")
                    if i > 0:
                        full_path_output_files.append(line[i+13:].strip()) 
            
            # execute provided test with list of files as arguments
            if extra_test_prog.endswith(".py"):
                cmd = [sys.executable, extra_test_prog]
            else:
                cmd = [extra_test_prog]
            cmd += full_path_output_files
            if len(command) > 1:
                cmd += ['--cmd'] + command[1:]
            
            rc = subprocess.run(cmd, text=True)
            if rc.returncode != 0:
                return 1  # extra test prog failed
            else:
                return 0
        elif not scenario_set.isdisjoint(set(["-k", "-q"])):
            min_no_of_lines = 4 # Metric files has initial few lines for headers etc..
            for output_file in output_files:
                root_dir = result_dir if is_result_dir else output_dir
                with open(os.path.join(root_dir, output_file), 'r') as outfile:
                    # Check to make sure file has counter values generated.
                    if len(outfile.readlines()) <= min_no_of_lines:
                        print(f"[ERROR] Metric file '{output_file}' is not having counters present.")
                        return 1 # Metric file should have more than 4 line present.
        elif scenario_set.isdisjoint(set(["--device-timing", "-d", "--device-timeline", "-t"])):
            min_no_of_lines = 4 # Metric files has initial few lines for headers etc..
            print(f"[INFO] Nothing to compare for {scenario_set} hence exiting early.", file = sys.stderr)
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
                
                # Without validated goldens only the content comparison is
                # skipped; the per-rank structural checks below still apply.
                expected_output_file = None if skip_golden_compare else \
                    extract_test_case_output(cmake_root_path, test_case_name, scenario_d_or_t)
                # Compare each sampled rank output
                all_passed = True
                for rank in ranks_list:
                    rank_output_file = None
                    for of in output_files:
                        if is_result_dir:
                            substr = f"rank_{rank}"
                            if substr in of:
                                rank_output_file = of
                                break
                        else:
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
                        if rank in ranks_sampled and device_sampled:
                            print(f"[ERROR] Output file for rank {rank} not found among generated files.", file = sys.stderr)
                            all_passed = False
                        continue
                    root_dir = result_dir if is_result_dir else output_dir  
                    if rank in ranks_sampled and device_sampled:
                        if skip_golden_compare:
                            print(f"[INFO] SILENCED: golden-output comparison "
                                  f"skipped for rank {rank} (--skip-golden-compare).")
                        else:
                            # Call unidiff comparison
                            comparison_result = call_unidiff(scenario_d_or_t, root_dir, rank_output_file, expected_output_file)
                            if comparison_result != 0:
                                print(f"[ERROR] Unidiff comparison failed for rank {rank}.", file = sys.stderr)
                                all_passed = False
                                break
                    else:
                        # check if rank_output_file is empty file or not
                        output_file_with_path = os.path.join(root_dir, rank_output_file)
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
                if skip_golden_compare:
                    print("[INFO] SILENCED: golden-output comparison skipped "
                          "(--skip-golden-compare); execution and output checks passed.")
                    return 0
                gold_file_to_compare = specific_test_case if specific_test_case is not None else test_case_name
                expected_output_file = extract_test_case_output(cmake_root_path, gold_file_to_compare, scenario_d_or_t)
                root_dir = result_dir if is_result_dir else output_dir  
                output_file_with_pid = max(output_files, key = lambda f: os.path.getmtime(os.path.join(root_dir, f)))

                # Call unidiff comparison
                comparison_result = call_unidiff(scenario_d_or_t, root_dir, output_file_with_pid, expected_output_file)
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
    parser.add_argument('-c', '--cmake_root_path', type=str, default=None,
                        help='Test source root holding the gold files. '
                             'Defaults to cpp_test next to this script, or python_test for python tests.')
    parser.add_argument('--test-bin-dir', type=str, default=None,
                        help='Directory holding the installed test binaries '
                             '(default: tools/unitrace/build/test/bin)')
    parser.add_argument('--output-dir', type=str, default=None,
                        help='Directory for results and metric scratch files '
                             '(default: tools/unitrace/build/test/output/<cpp_test|python_test>)')
    parser.add_argument('--results-subdir', type=str, default=None,
                        help='Write results under <output-dir>/results/<NAME> (test_unitrace.py '
                             'passes the tests.txt test name) instead of directly in results/')
    parser.add_argument('--mpiexec', action='store_true', help='Run unitrace with mpiexec')
    parser.add_argument('-n', '--num_ranks', type=int, default=1, help='Number of MPI ranks (default: 1)')
    parser.add_argument('-t','--test_case', type=str, default=None, help='Specific test case name (not the test executable name)')
    parser.add_argument('--unitrace-exe', type=str, default=None,
                        help='Path to the unitrace binary when it is not at <cmake_root>/../../build/unitrace')
    parser.add_argument('--skip-golden-compare', action='store_true',
                        help='Skip golden-output content comparisons (for environments without '
                             'validated golden files); execution and output checks still apply')
    parser.add_argument('-s', '--scenario', type=str, required=True,
                        help='Unitrace scenario options to run, or "native" to run the application without unitrace')
    parser.add_argument('args', nargs=argparse.REMAINDER, help='Application and its arguments (parsing stops here)')
    args = parser.parse_args()

    if not args.args:
        parser.error("[ERROR] Application command is required")
        return 1
    elif args.args[0] == "--":
        args.args = args.args[1:]

    scenario = args.scenario

    is_python = "python" in os.path.basename(args.args[0])
    if args.cmake_root_path is None:
        base = os.path.dirname(os.path.abspath(__file__))
        args.cmake_root_path = os.path.join(base, "python_test" if is_python else "cpp_test")
    if args.test_bin_dir is None:
        args.test_bin_dir = test_layout.bin_dir()
    if args.output_dir is None:
        args.output_dir = test_layout.output_dir(group="python_test" if is_python else "cpp_test")

    return run_unitrace(args.cmake_root_path, scenario, args.args[0], args.args[1:], 
                       args.extra_test_prog, args.test_bin_dir, args.output_dir,
                       args.mpiexec, args.num_ranks, args.test_case,
                       args.unitrace_exe, args.skip_golden_compare, args.results_subdir)

if __name__ == "__main__":
    sys.exit(main())
