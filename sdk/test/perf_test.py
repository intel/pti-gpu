from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Callable
from enum import Enum, auto
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
import tempfile
import io

# reconfigure stdout/stderr to utf-8 to support unicode characters in output
if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")
if isinstance(sys.stderr, io.TextIOWrapper):
    sys.stderr.reconfigure(encoding="utf-8")

# Sometimes we might run it with different number of threads
num_threads_str = "5"
skip_return_code = 77

# fmt: off
test_profiled = ["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", "50", "-s", "32",
                 "-c", "gpu", "-c", "sycl", "-c" , "overhead"]
test_prof_gpu = ["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", "50", "-s", "32",
                 "-c", "gpu"]
test_linkonly = ["dpc_gemm_threaded_linkonly", "-t", num_threads_str, "-r", "50", "-s", "32"]
test_baseline = ["dpc_gemm_threaded_baseline", "-t", num_threads_str, "-r", "50", "-s", "32"]

# For Overhead View test, we run the baseline with 1 thread and bigger GPU kernel.
# The test checks if the accumulated Overhead View time is at least
# the threshold ratio (test parameter) multiplied by the difference between
# the elapsed times of the baseline and the test under profiling.
test_overhead = ["dpc_gemm_threaded_profiled", "-t", "1", "-r", "200", "-s", "32",
                 "-c", "gpu", "-c",  "overhead"]
test_baseline_t_1 = ["dpc_gemm_threaded_baseline", "-t", "1", "-r", "200", "-s", "32"]

# Metrics Scope: the baseline is the same binary with GPU tracing only, so the
# measured overhead is the scope profiler alone. Each run launches more than
# 1000 kernels, past the per-pool query count, so coverage drops if query
# slots stop being recycled.
test_scope_baseline_t_1 = ["dpc_gemm_threaded_profiled", "-t", "1", "-r", "1200", "-s", "32",
                           "-c", "gpu"]
test_scope_t_1 = ["dpc_gemm_threaded_profiled", "-t", "1", "-r", "1200", "-s", "32",
                  "-c", "gpu", "-c", "scope"]
test_scope_baseline_t_2 = ["dpc_gemm_threaded_profiled", "-t", "2", "-r", "600", "-s", "32",
                           "-c", "gpu"]
test_scope_t_2 = ["dpc_gemm_threaded_profiled", "-t", "2", "-r", "600", "-s", "32",
                  "-c", "gpu", "-c", "scope"]
min_scope_coverage = 0.99

test_pc_sampling = ["pc_sampling_perf_profiled", "-r", "10", "-s", "2048"]
test_pc_sampling_baseline = ["pc_sampling_perf_baseline", "-r", "10", "-s", "2048"]

pc_sampling_profiled_phases = [
    "PC Sampling enable and configure time",
    "PC Sampling start collection time",
    "Workload execution time",
    "PC Sampling stop collection time",
    "PC Sampling post-processing time",
    "PC Sampling teardown time",
    "Total execution time",
]
pc_sampling_baseline_phases = ["Workload execution time", "Total execution time"]
pc_sampling_collection_phases = [
    "PC Sampling start collection time",
    "Workload execution time",
    "PC Sampling stop collection time",
]
pc_sampling_cpu_metrics = [
    "Workload user CPU time",
    "Workload system CPU time",
    "Workload total CPU time",
    "Workload CPU utilization",
]
# fmt: on


def analyze_default(threshold_overhead, _results, diff_med):
    if diff_med > threshold_overhead:
        print(
            f"\nTest failed - Measured overhead {diff_med:.2f}% exceeds threshold {threshold_overhead}%"
        )
        return 1

    print(
        f"\nTest passed - Measured overhead {diff_med:.2f}% is within threshold {threshold_overhead}%"
    )
    return 0


def collect_default_test_specific_values(_stdout_b, _stdout_t):
    return {}


def get_phase_timings(text, phases):
    timings = {phase: get_value(phase, text) for phase in phases}
    return timings if all(value is not None for value in timings.values()) else None


def collect_pc_sampling_values(stdout_b, stdout_t):
    profiled_timings = get_phase_timings(stdout_t, pc_sampling_profiled_phases)
    baseline_timings = get_phase_timings(stdout_b, pc_sampling_baseline_phases)
    if profiled_timings is None or baseline_timings is None:
        return None

    values = {
        "PC Sampling profiled " + phase: value
        for phase, value in profiled_timings.items()
    }
    values.update(
        {
            "PC Sampling baseline " + phase: value
            for phase, value in baseline_timings.items()
        }
    )

    profiled_cpu_timings = get_phase_timings(stdout_t, pc_sampling_cpu_metrics)
    baseline_cpu_timings = get_phase_timings(stdout_b, pc_sampling_cpu_metrics)
    if profiled_cpu_timings is not None and baseline_cpu_timings is not None:
        values.update(
            {
                "PC Sampling profiled " + metric: value
                for metric, value in profiled_cpu_timings.items()
            }
        )
        values.update(
            {
                "PC Sampling baseline " + metric: value
                for metric, value in baseline_cpu_timings.items()
            }
        )

    return values


@dataclass(frozen=True)
class TestConfig:
    baseline_command: list[str]
    test_command: list[str]
    repetitions: int = 15
    warm_up_runs: int = 1
    analyze: Callable[[float, "TestResults", float], int] = analyze_default
    collect_test_specific_values: Callable[[str, str], dict[str, float] | None] = (
        collect_default_test_specific_values
    )
    requires_throughput: bool = True
    # Pause after each test process, before the next baseline starts.
    settle_after_test_sec: float = 0.0


@dataclass
class TestResults:
    throughput_baseline: list[float]
    throughput_test: list[float]
    elapsed_baseline: list[float]
    elapsed_test: list[float]
    test_specific_values: dict[str, list[float]]


class CommandStatus(Enum):
    PASS = auto()
    FAIL = auto()
    SKIPPED = auto()
    HANGED = auto()


@dataclass
class CommandResult:
    status: CommandStatus
    stdout: str
    stderr: str


def analyze_overhead_view(threshold_overhead, results, _diff_med):
    captured_overhead_values = results.test_specific_values.get("Overhead time", [])
    if not (
        len(captured_overhead_values) == 1
        and len(results.elapsed_test) == 1
        and len(results.elapsed_baseline) == 1
    ):
        print("Test failed - Overhead analysis expects exactly one measurement")
        return 1
    captured_overhead_elapsed = captured_overhead_values[0]

    elapsed_diff = (float)(results.elapsed_test[0]) - (float)(
        results.elapsed_baseline[0]
    )
    print("\nElapsed time diff:                                 " + str(elapsed_diff))

    threshold_ratio = 0.01 * (  # percent -> ratio
        threshold_overhead  # collected L0 overhead expected to account,
    )
    print(
        "Expect captured Overhead View time to be at least: "
        + str(threshold_ratio * elapsed_diff)
        + " sec, Ratio: "
        + str(threshold_ratio)
    )
    if elapsed_diff <= 0.0:
        print("Test failed - Non-positive elapsed diff captured")
        return 1
    print(
        "Captured by PTI Overhead View overhead time:       "
        + str(captured_overhead_elapsed)
        + " sec, Ratio: "
        + str(captured_overhead_elapsed / elapsed_diff)
    )
    if captured_overhead_elapsed < threshold_ratio * elapsed_diff:
        print("Test failed - Too small Overhead View captured")
        return 1
    return 0


def collect_overhead_view_values(_stdout_b, stdout_t):
    captured_overhead = get_value("Overhead time", stdout_t)
    if captured_overhead is None:
        print("FAILED (no overhead data)")
        return None
    print(
        "Overhead due to profiling reported by PTI Overhead View (sec): "
        + str(captured_overhead)
    )
    return {"Overhead time": captured_overhead}


def format_duration(value):
    if value >= 1.0:
        return f"{value:.2f} s"
    return f"{value * 1_000.0:.1f} ms"


def print_pc_sampling_summary(results):
    profiled_medians = {
        phase: statistics.median(
            results.test_specific_values["PC Sampling profiled " + phase]
        )
        for phase in pc_sampling_profiled_phases
    }
    baseline_medians = {
        phase: statistics.median(
            results.test_specific_values["PC Sampling baseline " + phase]
        )
        for phase in pc_sampling_baseline_phases
    }

    print("\nPC Sampling phase summary (median):")
    for label, phase in [
        ("PC Sampling enable/configure", "PC Sampling enable and configure time"),
        ("Start collection", "PC Sampling start collection time"),
        ("Workload execution", "Workload execution time"),
        ("Stop collection", "PC Sampling stop collection time"),
        ("Post-processing", "PC Sampling post-processing time"),
        ("Teardown", "PC Sampling teardown time"),
        ("Total execution time", "Total execution time"),
    ]:
        print("  " + label.ljust(32) + format_duration(profiled_medians[phase]))

    print("\nNo PC Sampling (median):")
    for label, phase in [
        ("Workload execution", "Workload execution time"),
        ("Total execution time", "Total execution time"),
    ]:
        print("  " + label.ljust(32) + format_duration(baseline_medians[phase]))


def print_pc_sampling_cpu_summary(results):
    prefix = "PC Sampling profiled "
    if any(
        prefix + metric not in results.test_specific_values
        for metric in pc_sampling_cpu_metrics
    ):
        return

    print("\nWorkload CPU usage (median):")
    for label, metric in [
        ("User CPU time", "Workload user CPU time"),
        ("System CPU time", "Workload system CPU time"),
        ("Total CPU time", "Workload total CPU time"),
    ]:
        baseline = statistics.median(
            results.test_specific_values["PC Sampling baseline " + metric]
        )
        profiled = statistics.median(results.test_specific_values[prefix + metric])
        print(
            "  "
            + label.ljust(20)
            + format_duration(baseline)
            + " baseline, "
            + format_duration(profiled)
            + " PC Sampling"
        )

    baseline_utilization = statistics.median(
        results.test_specific_values["PC Sampling baseline Workload CPU utilization"]
    )
    profiled_utilization = statistics.median(
        results.test_specific_values[prefix + "Workload CPU utilization"]
    )
    print(
        f"  CPU utilization     {baseline_utilization:.1f}% baseline, "
        f"{profiled_utilization:.1f}% PC Sampling"
    )


def analyze_pc_sampling(threshold_overhead, results, _diff_med):
    profiled_values = [
        results.test_specific_values["PC Sampling profiled " + phase]
        for phase in pc_sampling_collection_phases
    ]
    profiled_elapsed = [sum(values) for values in zip(*profiled_values)]
    baseline_elapsed = results.test_specific_values[
        "PC Sampling baseline Workload execution time"
    ]
    median_profiled = statistics.median(remove_extreme_outliers(profiled_elapsed))
    median_baseline = statistics.median(remove_extreme_outliers(baseline_elapsed))
    diff_med = 100.0 * (median_profiled - median_baseline) / median_baseline

    print_pc_sampling_summary(results)
    print_pc_sampling_cpu_summary(results)
    return analyze_default(threshold_overhead, results, diff_med)


def collect_scope_values(_stdout_b, stdout_t):
    records = get_value("Scope records", stdout_t)
    kernels = get_value("Scope kernels", stdout_t)
    if records is None or not kernels:
        print("FAILED (no scope data)")
        return None
    return {"Scope coverage": records / kernels}


def analyze_scope(threshold_overhead, results, _diff_med):
    # Collecting fewer records makes a run faster, so check coverage before overhead.
    coverage = min(results.test_specific_values.get("Scope coverage", [0.0]))
    print(
        f"\nLowest scope record coverage: {coverage:.4f} (minimum {min_scope_coverage})"
    )
    if coverage < min_scope_coverage:
        print("\nTest failed - Scope records do not cover all kernels")
        return 1

    elapsed_baseline = statistics.median(results.elapsed_baseline)
    elapsed_test = statistics.median(results.elapsed_test)
    print("\nEnd-to-end latency, median across runs (sec):")
    print(f"  baseline {elapsed_baseline:.4f}  test {elapsed_test:.4f}")
    latency_increase = 100.0 * (elapsed_test - elapsed_baseline) / elapsed_baseline
    print("\n  baseline = PTI GPU tracing only (-c gpu)")
    print("  test     = PTI GPU tracing + Metrics Scope, XVE_ACTIVE (-c gpu -c scope)")
    print(
        "  => the increase is the cost of Metrics Scope on top of tracing, not vs. no PTI"
    )
    print(f"\nEnd-to-end latency increase (%): {latency_increase:.2f} (PRIMARY)")
    return analyze_default(threshold_overhead, results, latency_increase)


test_configs = {
    "profiled": TestConfig(test_baseline, test_profiled),
    "prof-gpu": TestConfig(test_baseline, test_prof_gpu),
    "linkonly": TestConfig(test_baseline, test_linkonly),
    "overhead": TestConfig(
        test_baseline_t_1,
        test_overhead,
        repetitions=1,
        warm_up_runs=0,
        analyze=analyze_overhead_view,
        collect_test_specific_values=collect_overhead_view_values,
    ),
    "scope": TestConfig(
        test_scope_baseline_t_1,
        test_scope_t_1,
        repetitions=9,
        analyze=analyze_scope,
        collect_test_specific_values=collect_scope_values,
        requires_throughput=False,
        settle_after_test_sec=1.0,
    ),
    "scope-mt": TestConfig(
        test_scope_baseline_t_2,
        test_scope_t_2,
        repetitions=9,
        analyze=analyze_scope,
        collect_test_specific_values=collect_scope_values,
        requires_throughput=False,
        settle_after_test_sec=1.0,
    ),
    "pc-sampling": TestConfig(
        test_pc_sampling_baseline,
        test_pc_sampling,
        repetitions=10,
        analyze=analyze_pc_sampling,
        collect_test_specific_values=collect_pc_sampling_values,
        requires_throughput=False,
    ),
}


def get_value(name, text):
    pattern = re.compile(
        name + r"\s*:\s+(\d*.?\d*e?-?\d*)"
    )  # number in float or exponential notation
    match = pattern.search(text)
    if match:
        return float(match.group(1))
    else:
        return None


def kill_process_tree(p):
    if sys.platform == "win32":
        # p.kill() only terminates the direct child; /T also takes its descendants
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/T", "/PID", str(p.pid)],
                capture_output=True,
                check=False,
                timeout=30,
            )
            if result.returncode == 0:
                return
            print(
                f"WARNING: taskkill failed for process {p.pid} "
                f"(exit code {result.returncode})"
            )
        except subprocess.TimeoutExpired:
            print(f"WARNING: taskkill timed out for process {p.pid}")
    p.kill()


def read_output(f):
    f.seek(0)
    return f.read().decode("latin-1")


def run_process(command, path, work_dir, timeout_sec, environ=None):
    command = command.copy()
    command[0] = os.path.join(os.path.abspath(path), command[0])

    # Output goes to temp files, not pipes: with pipes, a killed process that
    # is stuck in the GPU driver keeps the pipe open and reading its output
    # blocks forever (the test then hangs until the CI job is cancelled).
    # The process runs in work_dir, not in the binaries dir, so a stuck
    # process does not also hold that dir as its current directory.
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        p = subprocess.Popen(command, cwd=work_dir, env=environ, stdout=out, stderr=err)
        try:
            p.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            kill_process_tree(p)
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                print(f"WARNING: process {p.pid} did not exit after kill")
            print(f"ERROR: process timed out after {timeout_sec}s: {command[0]}")
            return CommandResult(
                CommandStatus.HANGED, read_output(out), read_output(err)
            )
        stdout, stderr = read_output(out), read_output(err)

    if p.returncode == 0:
        status = CommandStatus.PASS
    elif p.returncode == skip_return_code:
        status = CommandStatus.SKIPPED
    else:
        status = CommandStatus.FAIL
    return CommandResult(status, stdout, stderr)


def check_process_status(baseline_result, test_result):
    results = (baseline_result, test_result)
    if any(result.status == CommandStatus.FAIL for result in results):
        print("FAILED (benchmark process failed)")
        status = CommandStatus.FAIL
    elif any(result.status == CommandStatus.SKIPPED for result in results):
        print("SKIPPED (benchmark reported unsupported configuration)")
        status = CommandStatus.SKIPPED
    elif any(result.status == CommandStatus.HANGED for result in results):
        print("FAILED (benchmark process timed out)")
        status = CommandStatus.HANGED
    else:
        status = CommandStatus.PASS

    stderr_output = [result.stderr for result in results if result.stderr]
    if stderr_output:
        if status in (CommandStatus.PASS, CommandStatus.SKIPPED):
            print("WARNING (Detected stderr output)")
        for output in stderr_output:
            print(output)

    return status


def run_warm_up(config, path, work_dir, timeout_sec, environ):
    """Warm-up phase to stabilize CPU frequency and thermal state.

    Returns PASS, or the status of the first warm-up run that did not pass.
    """
    if config.warm_up_runs > 0:
        print("\nWarm-up phase:")
    for i in range(config.warm_up_runs):
        print("  Warm-up run " + str(i + 1) + "...", end=" ", flush=True)
        baseline_result = run_process(
            config.baseline_command, path, work_dir, timeout_sec, environ
        )
        test_result = run_process(
            config.test_command, path, work_dir, timeout_sec, environ
        )
        time.sleep(config.settle_after_test_sec)
        status = check_process_status(baseline_result, test_result)
        if status != CommandStatus.PASS:
            print("Warm-up terminated the test")
            return status
        print("✓")
    return CommandStatus.PASS


def run_test(path, work_dir, test_type, test_timeout=60):
    config = test_configs[test_type]
    environ = None
    if test_type == "pc-sampling":
        environ = {**os.environ, "ONEAPI_DEVICE_SELECTOR": "level_zero:0"}
    total_invocations = (config.repetitions + config.warm_up_runs) * 2
    timeout_sec = max(10, test_timeout // total_invocations)

    print("Test baseline command:    ", config.baseline_command)
    print("Test type " + test_type + " command: ", config.test_command)
    print("Repetitions: " + str(config.repetitions))
    print("Warm-up runs: " + str(config.warm_up_runs))
    print(
        f"Process timeout: {timeout_sec}s per invocation "
        f"(total budget: {test_timeout}s, invocations: {total_invocations})"
    )

    warm_up_status = run_warm_up(config, path, work_dir, timeout_sec, environ)
    if warm_up_status == CommandStatus.SKIPPED:
        return CommandStatus.SKIPPED, None
    if warm_up_status != CommandStatus.PASS:
        return CommandStatus.FAIL, None

    print("\nMeasurement phase - runs: ")
    throughput_test = []
    throughput_baseline = []
    elapsed_test = []
    elapsed_baseline = []
    test_specific_values = {}
    failed_measurements = 0
    for i in range(config.repetitions):
        # Test fails below anyway; stop so repeated hangs don't eat the
        # CI job timeout
        if failed_measurements * 2 >= config.repetitions:
            break
        print(str(i + 1) + "/" + str(config.repetitions) + ", ", end="", flush=True)
        # Interleaved: baseline first, then test (keeps CPU state similar)
        baseline_result = run_process(
            config.baseline_command, path, work_dir, timeout_sec, environ
        )
        test_result = run_process(
            config.test_command, path, work_dir, timeout_sec, environ
        )
        time.sleep(config.settle_after_test_sec)
        status = check_process_status(baseline_result, test_result)
        if status == CommandStatus.SKIPPED:
            return CommandStatus.SKIPPED, None
        elif status != CommandStatus.PASS:
            failed_measurements += 1
            continue

        tp_t = get_value("Throughput", test_result.stdout)
        tp_b = get_value("Throughput", baseline_result.stdout)
        el_t = get_value("Total execution time", test_result.stdout)
        el_b = get_value("Total execution time", baseline_result.stdout)

        if (
            el_t is None
            or el_b is None
            or (config.requires_throughput and (tp_t is None or tp_b is None))
        ):
            print("FAILED (no data)")
            failed_measurements += 1
            continue

        captured_test_values = config.collect_test_specific_values(
            baseline_result.stdout, test_result.stdout
        )
        if captured_test_values is None:
            failed_measurements += 1
            continue

        if config.requires_throughput:
            throughput_test.append(tp_t)
            throughput_baseline.append(tp_b)
        elapsed_test.append(el_t)
        elapsed_baseline.append(el_b)
        for name, value in captured_test_values.items():
            test_specific_values.setdefault(name, []).append(value)
    print()

    valid_measurements = config.repetitions - failed_measurements
    if valid_measurements * 2 <= config.repetitions:
        print(
            f"Too many measurement runs failed: {failed_measurements}/"
            f"{config.repetitions}"
        )
        return CommandStatus.FAIL, None
    if failed_measurements:
        print(
            f"Warning: some measurement runs failed: {failed_measurements}/"
            f"{config.repetitions}"
        )

    return CommandStatus.PASS, TestResults(
        throughput_baseline,
        throughput_test,
        elapsed_baseline,
        elapsed_test,
        test_specific_values,
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
    filtered_values = remove_extreme_outliers(values)
    standard_deviation = (
        statistics.stdev(filtered_values) if len(filtered_values) > 1 else 0.0
    )
    return (
        min(filtered_values),
        statistics.mean(filtered_values),
        statistics.median(filtered_values),
        max(filtered_values),
        standard_deviation,
    )


def analyze_results(
    test_type,
    threshold_overhead,
    results,
):
    config = test_configs[test_type]
    if not config.requires_throughput:
        return config.analyze(threshold_overhead, results, 0.0)

    print("Processing baseline results: ", end="")
    min_base, avg_base, med_base, max_base, std_base = process_data(
        results.throughput_baseline
    )
    print("Processing test results: ", end="")
    min_test, avg_test, med_test, max_test, std_test = process_data(
        results.throughput_test
    )

    units_str = "items/s"
    if test_type != "overhead":
        print("\nThreshold Overhead to pass: " + str(threshold_overhead) + "% =>")
        print(" Measured overhead should not exceed Threshold Overhead\n")
    else:
        units_str = "sec"
        print("\nThreshold Ratio to pass: " + str(threshold_overhead * 0.01) + " =>")
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

    return config.analyze(threshold_overhead, results, diff_med)


def parse_arguments(argv):
    if len(argv) < 4:
        print(
            f"Usage: {sys.argv[0]} <executable_path> <threshold> <test_type> [timeout]"
        )
        print("  executable_path  directory containing test binaries (required)")
        print("  threshold        overhead threshold % to pass (required)")
        print(f"  test_type        {' | '.join(test_configs)} (required)")
        print(
            "  timeout          approximate budget in seconds divided across all process invocations"
            " (10s floor per process), default: 60"
        )
        return None

    executable_path = argv[1]
    # An empty or missing directory almost always means the caller expanded an
    # undefined CMake variable. Report it here, otherwise the run dies in a bare
    # Popen traceback that never names the directory it was handed.
    if not executable_path:
        print("Empty executable_path: expected a directory containing test binaries")
        return None
    if not os.path.isdir(executable_path):
        print(f"executable_path '{executable_path}' is not an existing directory")
        return None

    threshold_overhead = float(argv[2])
    test_type = argv[3]
    if test_type not in test_configs:
        print(
            f"Invalid test type '{test_type}'. Expected one of: "
            f"{' | '.join(test_configs)}"
        )
        return None
    test_timeout = int(argv[4]) if len(argv) > 4 else 60
    return executable_path, threshold_overhead, test_type, test_timeout


def main():
    arguments = parse_arguments(sys.argv)
    if arguments is None:
        return 1

    executable_path, threshold_overhead, test_type, test_timeout = arguments
    print(" executable path: " + executable_path)

    work_dir = tempfile.mkdtemp(prefix="pti_perf_")
    try:
        result, test_results = run_test(
            executable_path, work_dir, test_type, test_timeout
        )
    finally:
        # A process that did not exit after kill may still hold the dir
        shutil.rmtree(work_dir, ignore_errors=True)
    if result is CommandStatus.SKIPPED:
        print("Test skipped")
        return skip_return_code
    if result is CommandStatus.FAIL:
        print("Test failed")
        return 1
    print("Test completed successfully")

    return analyze_results(test_type, threshold_overhead, test_results)


if __name__ == "__main__":
    sys.exit(main())
