# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================

import argparse
import sys
import csv
import re

def get_parser():
    parser = argparse.ArgumentParser(description = 'unitrace test diffing tool')

    parser.add_argument('--test', required = True, type = str, help = 'test run result file')
    parser.add_argument('--ref', required = True, type = str, help = 'reference run result file')
    parser.add_argument('--scenario', required = True, type = str, help ='unitrace test scenario')

    return parser.parse_args()

class ApiTiming:
    def __init__(self, file):
        self.lines = {}
        ignoring_functions = ["zeCommandList", "clEnqueue"]
        process_function_data = False

        with open(file, encoding='utf-8') as f:
            for line in f:
                line = line.strip()

                # Skip irrelevant lines
                if not line or any(skip in line for skip in [
                    "API Timing Summary",
                    "L0 Backend",
                    "CL GPU Backend",
                    "CL CPU Backend",
                    "Total Execution Time (ns):",
                    "Total API Time for L0 backend (ns):",
                    "Total API Time for CL GPU backend (ns):",
                    "Total API Time for CL CPU backend (ns):"
                ]):
                    process_function_data = False
                    continue

                # Detect header line
                if re.match(r'^Function\s*,', line):
                    process_function_data = True
                    continue

                if process_function_data:
                    # Use regex to split by comma but respect quoted strings
                    columns = [col.strip().strip('"') for col in re.split(r',(?=(?:[^"]*"[^"]*")*[^"]*$)', line)]

                    if len(columns) == 7:
                        function_name = columns[0]
                        if any(f in function_name for f in ignoring_functions):
                            continue
                        self.lines[function_name] = columns

    def compare(self, ref):
        mismatched_functions = 0
        total_functions = len(self.lines)

        for function_name, cur_row in self.lines.items():
            ref_row = ref.lines.get(function_name)

            if not ref_row:
                print(f"[WARNING] Function '{function_name}' not found in reference data.")
                continue

            cur_calls = int(cur_row[1].replace(",", "").strip())
            ref_calls = int(ref_row[1].replace(",", "").strip())

            if cur_calls != ref_calls:
                mismatched_functions += 1
                print(f"[WARNING] Calls mismatch for function '{function_name}':")
                print(f"[WARNING] Current: {cur_calls}")
                print(f"[WARNING] Reference: {ref_calls}")

        if total_functions > 0:
            mismatch_percentage = (mismatched_functions / total_functions) * 100
        else:
            print("[ERROR] Division by zero. 'total_functions' is zero.")
            return 3

        if mismatch_percentage >= 10:
            print(f"[ERROR] Test failed: {mismatched_functions} out of {total_functions} functions mismatched ({mismatch_percentage:.2f}%).")
            return 3
        else:
            print(f"[INFO] Test passed: {mismatched_functions} out of {total_functions} functions mismatched ({mismatch_percentage:.2f}%).")
            return 0

    def save_to_csv(self, ref, filename='output.csv'):
        field_names = [
            "No.",
            "Function",
            "Calls (Cur, Ref, Diff, Diff by Ref value(%)",
            "Time (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Time (%) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Average (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Min (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Max (ns) (Cur, Ref, Diff, Diff by Ref value(%)"
        ]

        with open(filename, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(field_names)

            for i, (function, cur_row) in enumerate(self.lines.items(), start=1):
                ref_row = ref.lines.get(function, ["0"] * 7)
                row = [i, function]

                for j in range(1, 7):
                    try:
                        cur_value = float(cur_row[j].replace(",", "").strip())
                        ref_value = float(ref_row[j].replace(",", "").strip())
                        diff = cur_value - ref_value
                        diff_percent = (diff / ref_value) * 100 if ref_value != 0 else 'N/A'

                        formatted = f"{cur_value}, {ref_value}, {diff}, {diff_percent:.2f}" if diff_percent != 'N/A' else f"{cur_value}, {ref_value}, {diff}, N/A"
                        row.append(formatted)
                    except (ValueError, IndexError):
                        row.append(f"{cur_row[j] if j < len(cur_row) else 'N/A'}, {ref_row[j] if j < len(ref_row) else 'N/A'}, N/A, N/A")

                writer.writerow(row)

        print(f"[INFO] Data saved to {filename}")
        
class DeviceTiming:
    def __init__(self, file):
        self.lines = {}
        ignoring_kernels = ["zeCommandList", "clEnqueue"]
        process_kernel_data = False

        with open(file, encoding='utf-8') as f:
            for line in f:
                line = line.strip()

                # Skip irrelevant lines
                if not line or any(skip in line for skip in [
                    "Device Timing Summary",
                    "L0 Backend",
                    "CL GPU Backend",
                    "CL CPU Backend",
                    "Total Execution Time (ns):",
                    "Total Device Time for CL GPU backend (ns):",
                    "Total Device Time for CL CPU backend (ns):"
                ]):
                    process_kernel_data = False
                    continue

                # Detect header line
                if re.match(r'^Kernel\s*,', line):
                    process_kernel_data = True
                    continue

                if process_kernel_data:
                    # Use regex to split by comma but respect quoted strings
                    columns = [col.strip().strip('"') for col in re.split(r',(?=(?:[^"]*"[^"]*")*[^"]*$)', line)]

                    if len(columns) == 7:
                        kernel_name = columns[0]
                        if any(k in kernel_name for k in ignoring_kernels):
                            continue
                        self.lines[kernel_name] = columns

    def compare(self, ref):
        mismatched_kernels = 0
        total_kernels = len(self.lines)

        for kernel_name, cur_row in self.lines.items():
            ref_row = ref.lines.get(kernel_name)

            if not ref_row:
                print(f"[WARNING] Kernel '{kernel_name}' not found in reference data.")
                continue

            cur_calls = int(cur_row[1].replace(",", "").strip())
            ref_calls = int(ref_row[1].replace(",", "").strip())

            if cur_calls != ref_calls:
                mismatched_kernels += 1
                print(f"[WARNING] Calls mismatch for kernel '{kernel_name}':")
                print(f"[WARNING] Current: {cur_calls}")
                print(f"[WARNING] Reference: {ref_calls}")

        if total_kernels > 0:
            mismatch_percentage = (mismatched_kernels / total_kernels) * 100
        else:
            print("[ERROR] Division by zero. 'total_kernels' is zero.")
            return 3

        if mismatch_percentage >= 10:
            print(f"[ERROR] Test failed: {mismatched_kernels} out of {total_kernels} kernels mismatched ({mismatch_percentage:.2f}%).")
            return 3
        else:
            print(f"[INFO] Test passed: {mismatched_kernels} out of {total_kernels} kernels mismatched ({mismatch_percentage:.2f}%).")
            return 0

    def save_to_csv(self, ref, filename='output.csv'):
        field_names = [
            "No.",
            "Kernel",
            "Calls (Cur, Ref, Diff, Diff by Ref value(%)",
            "Time (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Time (%) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Average (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Min (ns) (Cur, Ref, Diff, Diff by Ref value(%)",
            "Max (ns) (Cur, Ref, Diff, Diff by Ref value(%)"
        ]

        with open(filename, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(field_names)

            for i, (kernel, cur_row) in enumerate(self.lines.items(), start=1):
                ref_row = ref.lines.get(kernel, ["0"] * 7)
                row = [i, kernel]

                for j in range(1, 7):
                    try:
                        cur_value = float(cur_row[j].replace(",", "").strip())
                        ref_value = float(ref_row[j].replace(",", "").strip())
                        diff = cur_value - ref_value
                        diff_percent = (diff / ref_value) * 100 if ref_value != 0 else 'N/A'

                        formatted = f"{cur_value}, {ref_value}, {diff}, {diff_percent:.2f}" if diff_percent != 'N/A' else f"{cur_value}, {ref_value}, {diff}, N/A"
                        row.append(formatted)
                    except (ValueError, IndexError):
                        row.append(f"{cur_row[j] if j < len(cur_row) else 'N/A'}, {ref_row[j] if j < len(ref_row) else 'N/A'}, N/A, N/A")

                writer.writerow(row)

        print(f"[INFO] Data saved to {filename}")


class DeviceTimeline:
    def __init__(self, file):
        device_pattern = re.compile(r"Thread (\d+) Device (\S+) : (.+) \[ns\] (\d+) \(append\) (\d+) \(submit\) (\d+) \(start\) (\d+) \(end\)")
        ignore_patterns = ["zeCommandList", "clEnqueue"]
        self.kernels = []  # Initialize kernels list

        try:
            with open(file, encoding='utf-8') as f:
                lines = f.readlines()

                for i, line in enumerate(lines):
                    line = line.strip()

                    if not line:  # Skip empty lines
                        continue
                    match = device_pattern.match(line)
                    if match:
                        try:
                            keep = True
                            kernel = match.group(3).strip()
                            for k in ignore_patterns:
                                if k in kernel:
                                    keep = False
                                    continue
                            if keep:
                                self.kernels.append(kernel)

                        except ValueError as e:
                            print(f"[ERROR] processing line {i + 1}: {line}. [ERROR] {e}")
                    else:
                        print(f"[WARNING] Line {i + 1} did not match the expected format: {line}")

        except FileNotFoundError:
            print(f"[ERROR] The file {file} was not found.")
        except Exception as e:
            print(f"[ERROR] An unexpected error occurred while parsing {file}: {e}")

    def compare(self, ref):
        count_self = len(self.kernels)
        count_ref = len(ref.kernels)

        print(f"[INFO] Valid Kernel Count in self: {count_self}")
        print(f"[INFO] Valid Kernel Count in ref: {count_ref}")

        if count_self == count_ref:
            print("[INFO] Kernel counts are the same.")
            return 0  # Test passed
        else:
            print("[ERROR] Kernel counts differ.")
            return 3  # Test failed

    def save_to_csv(self, ref, filename='kernel_comparison.csv'):
        field_names = ["Kernel", "Kernel Count in Test File", "Kernel Count in Ref File"]

        with open(filename, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(field_names)

            all_kernels = list(set(self.kernels) | set(ref.kernels))

            for k in all_kernels:
                count_in_cur = sum(1 for entry in self.kernels if entry == k)
                count_in_ref = sum(1 for entry in ref.kernels if entry == k)
                row = [k, count_in_cur, count_in_ref]
                writer.writerow(row)

        print(f"[INFO] Data saved to {filename}")

def main():
    args = get_parser()
    result = 0

    if args.scenario in ("--device-timing", "-d"):
        device_timing_cur = DeviceTiming(args.test)
        device_timing_ref = DeviceTiming(args.ref)
        result = device_timing_cur.compare(device_timing_ref)

        if result == 0:
            print("[INFO] Device Comparison Passed")
        else:
            print("[ERROR] Device Comparison Failed")

        device_timing_cur.save_to_csv(device_timing_ref, filename = 'device_timing_results.csv')

    # yet to be invoked
    #if args.scenario in ("--host-timing", "-h"):
    #    api_cur = ApiTimeing(args.test)
    #    api_ref = ApiTimeing(args.ref)
    #    result = api_cur.compare(api_ref, scenario = args.scenario)

    #    if result == 0:
    #        print("[INFO] API Comparison Passed")
    #    else:
    #        print("[ERROR] API Comparison Failed")
    #
    #    api_cur.save_to_csv(api_cur.lines, api_ref.lines, filename = 'api_results.csv', scenario = args.scenario)

    if args.scenario in ("--device-timeline", "-t"):
        device_timeline_cur = DeviceTimeline(args.test)
        device_timeline_ref = DeviceTimeline(args.ref)
        result = device_timeline_cur.compare(device_timeline_ref)

        if result == 0:
            print("[INFO] Device Timeline Comparison Passed")
        else:
            print("[ERROR] Device Timeline Comparison Failed")

        device_timeline_cur.save_to_csv(device_timeline_ref, filename = 'device_timeline_results.csv')

    return result

if __name__ == '__main__':
    sys.exit(main())
