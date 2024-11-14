import os
import re
import sys
import subprocess
import traceback


test_profiled = [["dpc_gemm_threaded_profiled", "-t", "5", "-r", "50", "-s", "32"]]
test_linkonly = [["dpc_gemm_threaded_linkonly", "-t", "5", "-r", "50", "-s", "32"]]
test_baseline = [["dpc_gemm_threaded_baseline", "-t", "5", "-r", "50", "-s", "32"]]

# for overhead test we need to run the baseline with 1 thread and allow bigger GPU kernel
# the test checks if the reported Overhead time is at least ~ 40% (test parameter) of
# the elapsed time overhead (=difference between the baseline and the test under profiling)
test_overhead = [["dpc_gemm_threaded_overhead", "-t", "1", "-r", "100", "-s", "128"]]
test_baseline_t_1 = [
    ["dpc_gemm_threaded_baseline", "-t", "1", "-r", "100", "-s", "128"]
]


def get_value(name, text):
    pattern = re.compile(
        name + r"\s*:\s+(\d*.?\d*e?-?\d*)"
    )  # number in float or exponential notation
    match = pattern.search(text)
    if match:
        return float(match.group(1))
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
    command_baseline = test_baseline[0]
    command_test = test_profiled[0]
    if test_type == "profiled":
        command_test = test_profiled[0]
    elif test_type == "linkonly":
        command_test = test_linkonly[0]
    elif test_type == "overhead":
        command_test = test_overhead[0]
        command_baseline = test_baseline_t_1[0]
        repetitions = 1
    else:
        print("Invalid test type")
        return False, None, None, None, None

    print("Test baseline command:    ", command_baseline)
    print("Test type " + test_type + " command: ", command_test)
    print("Repetitions: " + str(repetitions))
    throughput_test = []
    throughput_baseline = []
    elapsed_test = []
    elapsed_baseline = []
    captured_overhead = None
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
        elapsed_test.append(get_value("Total execution time", stdout_t))
        elapsed_baseline.append(get_value("Total execution time", stdout_b))
        if test_type == "overhead":
            captured_overhead = get_value("Overhead time", stdout_t)
            print(
                "Overhead due to profiling reported by PTI Overhead View (sec): "
                + str(captured_overhead)
            )

    return (
        True,
        throughput_baseline,
        throughput_test,
        elapsed_baseline,
        elapsed_test,
        captured_overhead,
    )


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
    (
        Result,
        throughput_baseline,
        throughput_test,
        elapsed_baseline,
        elapsed_test,
        captured_overhead_elapsed,
    ) = run_test(executable_path, test_type)
    if not Result:
        print("Test failed")
        return 1
    else:
        min_base, avg_base, med_base, max_base = process_data(throughput_baseline)
        min_test, avg_test, med_test, max_test = process_data(throughput_test)
        if test_type != "overhead":
            print("\nThreshold Overhread to pass: " + str(threshold_overhead) + "% => ")
            print(
                " Thoughput of the test workload is less not more"
                " than Threshold Overhread\n"
            )
        else:
            print(
                "\nThreshold Ratio to pass: " + str(threshold_overhead * 0.01) + " => "
            )
            print(
                " Reported PTI Overhead View time should account for at least for "
                "Threshold Ratio of the Elapsed Time Diff between the test and the baseline\n"
            )

        print(
            "Baseline (sec): min "
            + format(min_base, ".2f")
            + " avg: "
            + format(avg_base, ".2f")
            + " med: "
            + format(med_base, ".2f")
            + " max: "
            + format(max_base, ".2f")
        )
        print(
            "Test (sec): min "
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

        if test_type == "overhead":
            if captured_overhead_elapsed is None:
                print("Test failed")
                return 1

            elapsed_diff = (float)(elapsed_test[0]) - (float)(elapsed_baseline[0])
            print(
                "\nElapsed time diff:                                 "
                + str(elapsed_diff)
            )

            threshold_ratio = 0.01 * (  # percent -> ratio
                threshold_overhead  # collected L0 overhead expected to account,
            )
            print(
                "Expect captured Overhead View time to be at least: "
                + str(threshold_ratio * elapsed_diff)
                + " sec, Ratio: "
                + str(threshold_ratio)
            )
            print(
                "Captured by PTI Overhead View overhead time:       "
                + str(captured_overhead_elapsed)
                + " sec, Ratio: "
                + str(captured_overhead_elapsed / elapsed_diff)
            )
            if captured_overhead_elapsed < threshold_ratio * elapsed_diff:
                print("Test failed - Too small overhead captured")
                return 1
            else:
                return 0

        if test_type != "overhead" and diff_med > threshold_overhead:
            print(f"Measured overhead {diff_med}, Threshold {threshold_overhead}")
            print("Test failed")
            return 1

        return 0


if __name__ == "__main__":
    sys.exit(main())
