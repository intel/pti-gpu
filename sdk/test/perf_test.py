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
    diff_max = 0.0
    diff_min = 1000000.0
    diff_avg = 0.0
    diffs = []
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
        throughput_test = get_value("Throughput", stdout_t)
        throughput_baseline = get_value("Throughput", stdout_b)
        diff = (
            100.0
            * (float)(throughput_baseline - throughput_test)
            / (float(throughput_baseline))
        )
        diffs.append(diff)
        diff_avg += diff
        diff_max = max(diff_max, diff)
        diff_min = min(diff_min, diff)

    diff_avg /= repetitions
    diff_med = sorted(diffs)[repetitions // 2]

    return True, diff_min, diff_avg, diff_med, diff_max


def main():
    executable_path = sys.argv[1]
    print(" executable path: " + executable_path)
    threshold_overhead = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    test_type = sys.argv[3] if len(sys.argv) > 3 else "profiled"
    Result, diff_min, diff_avg, diff_med, diff_max = run_test(
        executable_path, test_type
    )
    if not Result:
        print("Test failed")
        return 1
    else:
        print("Threshold Overhread to pass: " + str(threshold_overhead) + "%")
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
