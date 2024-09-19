import os
import re
import sys
import subprocess
import traceback


test_profiled = [["dpc_gemm_threaded_profiled", "-t", "5", "-r", "50", "-s", "32"]]
test_linkonly = [["dpc_gemm_threaded_linkonly", "-t", "5", "-r", "50", "-s", "32"]]
test_baseline = [["dpc_gemm_threaded_baseline", "-t", "5", "-r", "50", "-s", "32"]]


def get_value(name, text):
    pattern = re.compile(name + r"\s*:\s*(\d+)")
    match = pattern.search(text)
    if match:
        return int(match.group(1))
    else:
        return None


def run_process(command, path, environ=None):
    shell = False
    if sys.platform == "win32":
        shell = True
    else:
        command[0] = os.path.join(path, command[0])

    p = subprocess.Popen(
        command,
        cwd=path,
        shell=shell,
        env=environ,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="latin-1",
        text=True,
    )
    stdout, stderr = p.communicate()

    return stdout, stderr


def run_test(path, test_type="profiled", repetitions=11):
    print("Test baseline: ")
    command_baseline = test_baseline[0]
    print(command_baseline)
    command_test = test_profiled[0]
    if test_type == "profiled":
        command_test = test_profiled[0]
    elif test_type == "linkonly":
        command_test = test_linkonly[0]
    else:
        print("Invalid test type")
        return False, None, None, None, None

    print("Test type: " + test_type + " Command: ")
    print(command_test)
    print("Repetitions: " + str(repetitions))
    throughput_test = []
    throughput_baseline = []
    for i in range(repetitions):
        command = path
        print("Running test: " + str(i))
        (stdout_t, stderr_t) = run_process(command_test, path)
        (stdout_b, stderr_b) = run_process(command_baseline, path)
        if stderr_t or stderr_b:
            print(stderr_t)
            print(stderr_b)
            return False, None, None, None, None

        print(stdout_t)
        print(stdout_b)
        throughput_test.append(get_value("Throughput", stdout_t))
        throughput_baseline.append(get_value("Throughput", stdout_b))

    return True, throughput_baseline, throughput_test


def process_data(values):
    max_v = 0.0
    min_v = 1000000.0
    avg_v = 0.0
    count = len(values)
    for i in range(count):
        avg_v += values[i]
        max_v = max(max_v, values[i])
        min_v = min(min_v, values[i])

    avg_v /= count
    med_v = sorted(values)[count // 2]

    return min_v, avg_v, med_v, max_v


def main():
    executable_path = sys.argv[1]
    print(" executable path: " + executable_path)
    threshold_overhead = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    test_type = sys.argv[3] if len(sys.argv) > 3 else "profiled"
    Result, throughput_baseline, throughput_test = run_test(executable_path, test_type)
    if not Result:
        print("Test failed")
        return 1
    else:
        min_base, avg_base, med_base, max_base = process_data(throughput_baseline)
        min_test, avg_test, med_test, max_test = process_data(throughput_test)
        print("Threshold Overhread to pass: " + str(threshold_overhead) + "%")
        print(
            "Baseline: min "
            + format(min_base, ".2f")
            + " avg: "
            + format(avg_base, ".2f")
            + " med: "
            + format(med_base, ".2f")
            + " max: "
            + format(max_base, ".2f")
        )
        print(
            "Test: min "
            + format(min_test, ".2f")
            + " avg: "
            + format(avg_test, ".2f")
            + " med: "
            + format(med_test, ".2f")
            + " max: "
            + format(max_test, ".2f")
        )

        diff_med = 100.0 * (float)(med_base - med_test) / (float(med_base))
        diff_min = 100.0 * (float)(min_base - min_test) / (float(min_base))
        diff_max = 100.0 * (float)(max_base - max_test) / (float(max_base))
        diff_avg = 100.0 * (float)(avg_base - avg_test) / (float(avg_base))
        print(
            "Overhead: min "
            + format(diff_min, ".2f")
            + " avg: "
            + format(diff_avg, ".2f")
            + " med: "
            + format(diff_med, ".2f")
            + " max: "
            + format(diff_max, ".2f")
        )

        if diff_med > threshold_overhead:
            print("Test failed")
            return 1

        return 0


if __name__ == "__main__":
    sys.exit(main())
