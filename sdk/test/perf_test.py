import os
import re
import sys
import subprocess
import traceback
import statistics

# reconfigure stdout/stderr to utf-8 to support unicode characters in output
sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

# Strings that could be dumped to stderr due to different Level-Zero driver versions behavior
expected_stderr_strings = [
    "ZE_LOADER_DEBUG_TRACE:zeInitDrivers called first, "
    "but not supported by driver, returning uninitialized."
]

# Sometimes we might run it with different number of threads
num_threads_str = "5"

# fmt: off
test_profiled = [["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", "50", "-s", "32",
                  "-c", "gpu", "-c", "sycl", "-c" , "overhead"]]
test_prof_gpu = [["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", "50", "-s", "32",
                  "-c", "gpu"]]
test_linkonly = [["dpc_gemm_threaded_linkonly", "-t", num_threads_str, "-r", "50", "-s", "32"]]
test_baseline = [["dpc_gemm_threaded_baseline", "-t", num_threads_str, "-r", "50", "-s", "32"]]

# For Overhead View test, we run the baseline with 1 thread and bigger GPU kernel.
# The test checks if the accumulated Overhead View time is at least
# the threshold ratio (test parameter) multiplied by the difference between
# the elapsed times of the baseline and the test under profiling.
test_overhead = [["dpc_gemm_threaded_profiled", "-t", "1", "-r", "200", "-s", "32",
                  "-c", "gpu", "-c",  "overhead"]]
test_baseline_t_1 = [["dpc_gemm_threaded_baseline", "-t", "1", "-r", "200", "-s", "32"]]
# fmt: on


def check_expected_content(text):
    """
    Check if the text contains expected strings
    """
    for line in text.splitlines():
        if not (line in expected_stderr_strings):
            return False
    return True


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


def run_test(path, test_type="profiled", repetitions=15, warm_up_runs=1):
    command_baseline = test_baseline[0]
    command_test = test_profiled[0]
    if test_type == "profiled":
        command_test = test_profiled[0]
    elif test_type == "prof-gpu":
        command_test = test_prof_gpu[0]
    elif test_type == "linkonly":
        command_test = test_linkonly[0]
    elif test_type == "overhead":
        command_test = test_overhead[0]
        command_baseline = test_baseline_t_1[0]
        repetitions = 1
        warm_up_runs = 0  # No warm-up for overhead test
    else:
        print("Invalid test type")
        return False, None, None, None, None, None

    print("Test baseline command:    ", command_baseline)
    print("Test type " + test_type + " command: ", command_test)
    print("Repetitions: " + str(repetitions))
    print("Warm-up runs: " + str(warm_up_runs))

    # Warm-up phase to stabilize CPU frequency and thermal state
    if warm_up_runs > 0:
        print("\nWarm-up phase:")
        for i in range(warm_up_runs):
            print("  Warm-up run " + str(i + 1) + "...", end=" ", flush=True)
            run_process(command_baseline, path)
            run_process(command_test, path)
            print("✓")

    print("\nMeasurement phase - runs: ")
    throughput_test = []
    throughput_baseline = []
    elapsed_test = []
    elapsed_baseline = []
    captured_overhead = None
    for i in range(repetitions):
        print(str(i + 1) + "/" + str(repetitions) + ", ", end="", flush=True)
        # Interleaved: baseline first, then test (keeps CPU state similar)
        stdout_b, stderr_b = run_process(command_baseline, path)
        stdout_t, stderr_t = run_process(command_test, path)
        if stderr_t or stderr_b:
            print("WARNING (Detected stderr output)")
            print(stderr_t)
            print(stderr_b)
            # If something not expected detected in stderr - fail the test
            # if some new benign stderr detected - add it to the expected list
            if not (check_expected_content(stderr_t)):
                print("Test failed due to unexpected stderr content (see above)")
                return False, None, None, None, None, None

            print("stderr content was expected - proceeding with the test...")

        tp_t = get_value("Throughput", stdout_t)
        tp_b = get_value("Throughput", stdout_b)
        el_t = get_value("Total execution time", stdout_t)
        el_b = get_value("Total execution time", stdout_b)

        if tp_t is None or tp_b is None or el_t is None or el_b is None:
            print("FAILED (no data)")
            continue

        throughput_test.append(tp_t)
        throughput_baseline.append(tp_b)
        elapsed_test.append(el_t)
        elapsed_baseline.append(el_b)

        if test_type == "overhead":
            captured_overhead = get_value("Overhead time", stdout_t)
            print(
                "Overhead due to profiling reported by PTI Overhead View (sec): "
                + str(captured_overhead)
            )
    print()
    return (
        True,
        throughput_baseline,
        throughput_test,
        elapsed_baseline,
        elapsed_test,
        captured_overhead,
    )


def remove_extreme_outliers(values):
    """
    Remove top and bottom 2.5% extreme outliers.
    This handles occasional context switches, thermal spikes, etc.

    With fewer than 4 values, percentile-based trimming is not meaningful:
    we would either remove too much data or skew the results, so in that
    case the input is returned unchanged.
    """
    if len(values) < 4:
        return values

    sorted_vals = sorted(values)
    cutoff = max(1, len(sorted_vals) // 40)  # ~2.5% on each end
    removed = 2 * cutoff
    if removed > 0:
        print(f"removed {removed} extreme outliers")
    return sorted_vals[cutoff:-cutoff]


def process_data(values):
    max_v = 0.0
    min_v = 1000000.0
    avg_v = 0.0
    count = len(values)
    # With 2025.0.4 compiler started to get some random failures on Win when profiling -
    # - adding processing of such - for now ignoring their data
    # TODO to investigate
    valid_count = count
    valid_values = []
    for i in range(count):
        if not (values[i] is None):
            valid_values.append(values[i])
            avg_v += values[i]
            max_v = max(max_v, values[i])
            min_v = min(min_v, values[i])
        else:
            valid_count -= 1

    if valid_count <= count / 2:
        print(
            f"Too many runs failed, count of runs: {count}, valid of them: {valid_count}"
        )
        return None, None, None, None, None
    elif valid_count < count:
        print(
            f"Warning: some runs failed, count of runs: {count}, valid of them: {valid_count}"
        )

    # Remove extreme outliers to get more robust statistics
    valid_values = remove_extreme_outliers(valid_values)

    avg_v = sum(valid_values) / len(valid_values)
    med_v = statistics.median(valid_values)

    # Calculate standard deviation
    if len(valid_values) > 1:
        std_v = statistics.stdev(valid_values)
    else:
        std_v = 0.0

    return min_v, avg_v, med_v, max_v, std_v


def main():
    executable_path = sys.argv[1]
    print(" executable path: " + executable_path)
    threshold_overhead = float(sys.argv[2]) if len(sys.argv) > 2 else 60
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
        print("Processing baseline results: ", end="")
        min_base, avg_base, med_base, max_base, std_base = process_data(
            throughput_baseline
        )
        print("Processing test results: ", end="")
        min_test, avg_test, med_test, max_test, std_test = process_data(throughput_test)

        if (
            min_base is None
            or avg_base is None
            or med_base is None
            or max_base is None
            or min_test is None
            or avg_test is None
            or med_test is None
            or max_test is None
        ):
            print("Test failed")
            return 1

        units_str = "items/s"
        if test_type != "overhead":
            print("\nThreshold Overhead to pass: " + str(threshold_overhead) + "% =>")
            print(" Measured overhead should not exceed Threshold Overhead\n")
        else:
            units_str = "sec"
            print(
                "\nThreshold Ratio to pass: " + str(threshold_overhead * 0.01) + " =>"
            )
            print(
                " Reported PTI Overhead View time should account for at least for "
                "Threshold Ratio of the Elapsed Time Diff between the test and the baseline\n"
            )

        print(
            "Baseline ("
            + units_str
            + "): min "
            + format(min_base, ".2f")
            + " avg: "
            + format(avg_base, ".2f")
            + " ± "
            + format(std_base, ".2f")
            + " med: "
            + format(med_base, ".2f")
            + " max: "
            + format(max_base, ".2f")
        )
        print(
            "Test ("
            + units_str
            + "):     min "
            + format(min_test, ".2f")
            + " avg: "
            + format(avg_test, ".2f")
            + " ± "
            + format(std_test, ".2f")
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
            "\nOverhead (%): med: "
            + format(diff_med, ".2f")
            + " (PRIMARY) avg: "
            + format(diff_avg, ".2f")
            + " min: "
            + format(diff_min, ".2f")
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
            if elapsed_diff < 0.0:
                print("Test failed - Negative elapsed diff captured")
                return 1
            elif captured_overhead_elapsed < threshold_ratio * elapsed_diff:
                print("Test failed - Too small Overhead View captured")
                return 1
            else:
                return 0

        if test_type != "overhead" and diff_med > threshold_overhead:
            print(
                f"\nTest failed - Measured overhead {diff_med:.2f}% exceeds threshold {threshold_overhead}%"
            )
            return 1

        print(
            f"\nTest passed - Measured overhead {diff_med:.2f}% is within threshold {threshold_overhead}%"
        )
        return 0


if __name__ == "__main__":
    sys.exit(main())
