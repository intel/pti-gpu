from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Callable
from enum import Enum, auto
import glob
import math
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
# The timed window is short (tens of ms), so each process first runs untimed warm-up
# iterations (-w) to get the GPU clock, JIT and driver pools to steady state.
repeat_str = "100"
warmup_str = "100"
test_profiled = ["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", repeat_str,
                 "-w", warmup_str, "-s", "32", "-c", "gpu", "-c", "sycl", "-c" , "overhead"]
test_prof_gpu = ["dpc_gemm_threaded_profiled", "-t", num_threads_str, "-r", repeat_str,
                 "-w", warmup_str, "-s", "32", "-c", "gpu"]
test_baseline = ["dpc_gemm_threaded_baseline", "-t", num_threads_str, "-r", repeat_str,
                 "-w", warmup_str, "-s", "32"]
# The link-only overhead is expected to be ~0 and its threshold is tight (a few %), so a
# window of 100 iterations (~30-80 ms) leaves CPU idle-state/frequency jitter (several %)
# comparable to the threshold. A longer timed window averages that jitter out.
linkonly_repeat_str = "500"
test_linkonly = ["dpc_gemm_threaded_linkonly", "-t", num_threads_str, "-r", linkonly_repeat_str,
                 "-w", warmup_str, "-s", "32"]
test_linkonly_baseline = ["dpc_gemm_threaded_baseline", "-t", num_threads_str,
                          "-r", linkonly_repeat_str, "-w", warmup_str, "-s", "32"]

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
    # Keep CPUs out of deep idle states while measuring (see hold_cpu_latency_qos).
    limit_cpu_idle: bool = False
    # Decide pass/fail on the per-iteration test/baseline ratio and its uncertainty
    # (see analyze_paired) instead of on the ratio of the two separate medians.
    paired_analysis: bool = False
    # With paired_analysis: while the threshold lies inside the confidence interval, keep
    # adding measurement pairs in batches, up to this total (0: no adaptive repetitions).
    max_repetitions: int = 0


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
    median_profiled = statistics.median(
        remove_extreme_outliers(profiled_elapsed, "profiled")
    )
    median_baseline = statistics.median(
        remove_extreme_outliers(baseline_elapsed, "baseline")
    )
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


CONFIDENCE = 0.95
# Baseline run-to-run variation up to which the machine counts as quiet in the UNDECIDED message.
QUIET_MACHINE_NOISE_PERCENT = 5.0
# Adaptive repetitions for paired tests: batch size and the largest total of pairs.
# The interval is checked after each batch (here after 15, 30 and 45 pairs).
ADAPTIVE_BATCH_PAIRS = 15
ADAPTIVE_MAX_PAIRS = 45


def interval_checks(config):
    """How many times a paired test checks its interval (1 without adaptive repetitions)."""
    extra = max(0, config.max_repetitions - config.repetitions)
    return 1 + math.ceil(extra / ADAPTIVE_BATCH_PAIRS)


def per_check_confidence(config):
    """Confidence of each interval check so that all checks together keep CONFIDENCE.

    Stopping at the first check whose interval excludes the threshold gives the noise one
    chance per check to produce a wrong "confident" result. Splitting the error budget
    1 - CONFIDENCE evenly over the checks (Bonferroni) bounds the chance that any of them
    is wrong by 1 - CONFIDENCE, whatever happens between the checks.
    """
    return 1.0 - (1.0 - CONFIDENCE) / interval_checks(config)


test_configs = {
    # Same host-latency-bound gemm workload as linkonly, so the same noise reduction and
    # paired analysis apply; the timed window stays at 100 iterations (wider thresholds).
    "profiled": TestConfig(
        test_baseline,
        test_profiled,
        limit_cpu_idle=True,
        paired_analysis=True,
        max_repetitions=ADAPTIVE_MAX_PAIRS,
    ),
    "prof-gpu": TestConfig(
        test_baseline,
        test_prof_gpu,
        limit_cpu_idle=True,
        paired_analysis=True,
        max_repetitions=ADAPTIVE_MAX_PAIRS,
    ),
    "linkonly": TestConfig(
        test_linkonly_baseline,
        test_linkonly,
        limit_cpu_idle=True,
        paired_analysis=True,
        max_repetitions=ADAPTIVE_MAX_PAIRS,
    ),
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


CPU_DMA_LATENCY_DEV = "/dev/cpu_dma_latency"


def hold_cpu_latency_qos():
    """Ask the kernel (PM QoS) to keep all CPUs out of idle states with non-zero exit latency.

    The gemm workload is host-latency bound: every iteration submits a tiny kernel and blocks
    on it for ~0.1 ms, during which the core idles. With deep C-states enabled (exit latency
    up to ~1 ms on some client CPUs) whether a core wakes up fast or slow is random, which
    makes whole runs 2-3x faster or slower and swamps a threshold of a few percent.

    The request is active while the returned file descriptor is open and is dropped by the
    kernel automatically when it is closed or the process exits, so nothing persists.
    Returns the descriptor, or None when unavailable (non-Linux, or no write access: the
    device is root-only by default).
    """
    if not sys.platform.startswith("linux"):
        return None
    try:
        fd = os.open(CPU_DMA_LATENCY_DEV, os.O_WRONLY)
    except OSError as e:
        print(
            f"\nWARNING: cannot limit CPU idle states ({CPU_DMA_LATENCY_DEV}: {e.strerror}); "
            "deep C-states make results noisy.\n"
            f"  Grant access e.g. with: sudo setfacl -m u:$USER:w {CPU_DMA_LATENCY_DEV}"
        )
        return None
    try:
        os.write(fd, (0).to_bytes(4, sys.byteorder, signed=True))
    except OSError as e:
        os.close(fd)
        print(
            f"\nWARNING: cannot limit CPU idle states ({CPU_DMA_LATENCY_DEV}: {e.strerror})"
        )
        return None
    print(
        f"\nCPU idle states limited for the measurement ({CPU_DMA_LATENCY_DEV} = 0 us)"
    )
    return fd


def read_first_line(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.readline().strip()
    except OSError:
        return None


def print_system_state(when):
    """Snapshot of what affects the measurement: CPU load and frequency policy, GPU clocks
    and the busiest processes. Linux only; anything unavailable is skipped."""
    if not sys.platform.startswith("linux"):
        print(f"\nSystem state ({when}): not collected on {sys.platform}")
        return
    print(f"\nSystem state ({when}):")
    if hasattr(os, "getloadavg"):
        load = os.getloadavg()
        print(
            f"  CPU: {os.cpu_count()} logical CPUs, load average "
            f"{load[0]:.2f} / {load[1]:.2f} / {load[2]:.2f} (1/5/15 min)"
        )
        if load[0] > 0.25 * (os.cpu_count() or 1):
            print("  WARNING: other workloads are running; results will be noisy")
    governor = read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")
    if governor:
        print(f"  CPU frequency governor: {governor}")
    # xe driver: .../tile*/gt*/freq0/{min,max,act,cur}_freq; i915: gt_{min,max,act,cur}_freq_mhz
    for freq_dir in sorted(glob.glob("/sys/class/drm/card*/device/tile*/gt*/freq0")):
        values = {
            name: read_first_line(os.path.join(freq_dir, name + "_freq"))
            for name in ("min", "max", "act", "cur")
        }
        if not any(values.values()):
            continue
        print(
            f"  GPU {freq_dir.split('/')[4]}/{freq_dir.split('/')[6]}/{freq_dir.split('/')[7]} "
            "freq MHz: " + ", ".join(f"{k} {v}" for k, v in values.items() if v)
        )
    # card<N> only: card* also matches connectors such as card0-DP-1
    cards = [
        c
        for c in glob.glob("/sys/class/drm/card*")
        if re.fullmatch(r"card\d+", os.path.basename(c))
    ]
    for card in sorted(cards, key=lambda c: int(os.path.basename(c)[4:])):
        values = {
            name: read_first_line(os.path.join(card, f"gt_{name}_freq_mhz"))
            for name in ("min", "max", "act", "cur")
        }
        if any(values.values()):
            print(
                f"  GPU {os.path.basename(card)} freq MHz: "
                + ", ".join(f"{k} {v}" for k, v in values.items() if v)
            )
    try:
        top = subprocess.run(
            ["ps", "-eo", "pcpu,user,comm", "--sort=-pcpu", "--no-headers"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        ).stdout.splitlines()[:5]
        print(
            "  Busiest processes (%CPU user command): "
            + "; ".join(t.strip() for t in top)
        )
    except (OSError, subprocess.SubprocessError):
        pass


def print_raw_runs(baseline, test):
    """Per-run values in run order, to see streaks (e.g. a slow phase of the machine)."""
    print("\nRaw throughput per measurement run (items/s), in run order:")
    print("  run   baseline       test   test/baseline")
    for i, (b, t) in enumerate(zip(baseline, test), start=1):
        ratio = t / b if b else float("nan")
        print(f"  {i:3d} {b:10.0f} {t:10.0f}   {ratio:6.3f}")
    for name, values in (("baseline", baseline), ("test", test)):
        if values and min(values) > 0:
            spread = max(values) / min(values)
            note = "  <- runs disagree by more than 1.5x" if spread > 1.5 else ""
            print(f"  {name}: max/min {spread:.2f}{note}")


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


def run_test(path, work_dir, test_type, test_timeout=60, threshold_overhead=None):
    config = test_configs[test_type]
    environ = None
    if test_type == "pc-sampling":
        environ = {**os.environ, "ONEAPI_DEVICE_SELECTOR": "level_zero:0"}
    # Paired tests may add pairs up to max_repetitions; budget for all of them.
    total_invocations = (
        max(config.repetitions, config.max_repetitions) + config.warm_up_runs
    ) * 2
    timeout_sec = max(10, test_timeout // total_invocations)

    print("Test baseline command:    ", config.baseline_command)
    print("Test type " + test_type + " command: ", config.test_command)

    def option_value(command, *names):
        for i, arg in enumerate(command[:-1]):
            if arg in names:
                return command[i + 1]
        return None

    repetitions = str(config.repetitions)
    if config.paired_analysis and config.max_repetitions > config.repetitions:
        checks = ", ".join(
            str(
                min(
                    config.repetitions + i * ADAPTIVE_BATCH_PAIRS,
                    config.max_repetitions,
                )
            )
            for i in range(interval_checks(config))
        )
        repetitions += (
            f" (adaptive: interval checked after {checks} pairs while undecided, "
            f"{per_check_confidence(config):.1%} per check)"
        )
    print("Repetitions (measured baseline+test pairs): " + repetitions)
    print(
        "Warm-up runs (untimed baseline+test pairs before measuring): "
        + str(config.warm_up_runs)
    )
    for label, command in (
        ("baseline", config.baseline_command),
        ("test", config.test_command),
    ):
        warmup = option_value(command, "-w", "--warmup")
        timed = option_value(command, "-r", "--repeat")
        if timed is not None:
            print(
                f"Iterations per thread in each {label} run: "
                f"{warmup if warmup is not None else 1} warm-up (untimed), {timed} timed"
            )
    print(
        f"Process timeout: {timeout_sec}s per invocation "
        f"(total budget: {test_timeout}s, invocations: {total_invocations})"
    )

    print_system_state("before the runs")
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
    adaptive = (
        config.paired_analysis
        and config.max_repetitions > config.repetitions
        and threshold_overhead is not None
    )
    target = config.repetitions
    attempted = 0
    while True:
        # Test fails below anyway; stop so repeated hangs don't eat the
        # CI job timeout
        if failed_measurements * 2 >= target:
            break
        if attempted == target:
            # Adaptive repetitions: when a batch is done (whether its last attempt succeeded
            # or not) and the threshold still lies inside the interval, measure another batch,
            # up to max_repetitions attempts.
            if not adaptive or target >= config.max_repetitions:
                break
            verdict = paired_verdict(
                threshold_overhead,
                throughput_baseline,
                throughput_test,
                per_check_confidence(config),
            )
            if verdict != "UNDECIDED":
                break
            target = min(target + ADAPTIVE_BATCH_PAIRS, config.max_repetitions)
            print(
                f"\n  undecided after {len(throughput_test)} pairs: adding "
                f"{target - attempted} more (up to {config.max_repetitions})"
            )
        attempted += 1
        print(str(attempted) + "/" + str(target) + ", ", end="", flush=True)
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
    print_system_state("after the runs")

    valid_measurements = attempted - failed_measurements
    if valid_measurements * 2 <= attempted:
        print(f"Too many measurement runs failed: {failed_measurements}/{attempted}")
        return CommandStatus.FAIL, None
    if failed_measurements:
        print(
            f"Warning: some measurement runs failed: {failed_measurements}/{attempted}"
        )

    return CommandStatus.PASS, TestResults(
        throughput_baseline,
        throughput_test,
        elapsed_baseline,
        elapsed_test,
        test_specific_values,
    )


def quartiles(sorted_vals):
    """Q1 and Q3 by linear interpolation (same as statistics.quantiles 'inclusive')."""
    q = statistics.quantiles(sorted_vals, n=4, method="inclusive")
    return q[0], q[2]


def remove_extreme_outliers(values, label=""):
    """
    Remove outliers by the interquartile-range rule: values below Q1 - 1.5*IQR or above
    Q3 + 1.5*IQR are dropped. Unlike trimming a fixed share from each end, this removes as
    many runs as are actually off (e.g. a streak of slow runs when other workloads compete
    for the machine) and nothing when the runs agree.

    With fewer than 4 values quartiles are not meaningful and the input is returned unchanged.
    Prints what was removed so that a run on another machine can be understood from the log.
    """
    if len(values) < 4:
        return values

    sorted_vals = sorted(values)
    q1, q3 = quartiles(sorted_vals)
    iqr = q3 - q1
    low, high = q1 - 1.5 * iqr, q3 + 1.5 * iqr
    kept = [v for v in sorted_vals if low <= v <= high]
    removed = [v for v in sorted_vals if v < low or v > high]
    prefix = f"  [{label}] " if label else "  "
    print(
        f"{prefix}IQR trimming: Q1 {q1:.2f}, Q3 {q3:.2f}, IQR {iqr:.2f}, "
        f"kept range [{low:.2f}, {high:.2f}], removed {len(removed)}/{len(values)}"
        + (f": {', '.join(format(v, '.2f') for v in removed)}" if removed else "")
    )
    # Defensive: never let trimming empty the set (cannot happen with the 1.5*IQR rule,
    # since Q1..Q3 always holds values, but keep the guarantee explicit).
    return kept if kept else sorted_vals


def process_data(values, label=""):
    filtered_values = remove_extreme_outliers(values, label)
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

    print_raw_runs(results.throughput_baseline, results.throughput_test)

    # Paired tests decide on the untrimmed per-pair overheads only; the separate-median
    # statistics below (IQR-trimmed) would print a second, unused result, so skip them.
    if config.paired_analysis:
        print(f"\nThreshold Overhead to pass: {threshold_overhead}%")
        return analyze_paired(
            threshold_overhead,
            results.throughput_baseline,
            results.throughput_test,
            per_check_confidence(config),
            interval_checks(config),
        )

    print("\nProcessing results:")
    min_base, avg_base, med_base, max_base, std_base = process_data(
        results.throughput_baseline, "baseline"
    )
    min_test, avg_test, med_test, max_test, std_test = process_data(
        results.throughput_test, "test"
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

    # The overhead is the relative throughput loss of the test against the baseline. Only the
    # medians decide pass/fail. The other two numbers pair up the same statistic of both runs
    # (the slowest run of each, the fastest run of each) and are printed to show the spread;
    # they are not a min/max of the overhead, so either of them can be above or below the
    # median, or negative when the test happened to have the faster run.
    diff_med = 100.0 * (med_base - med_test) / med_base
    diff_avg = 100.0 * (avg_base - avg_test) / avg_base
    diff_slowest = 100.0 * (min_base - min_test) / min_base
    diff_fastest = 100.0 * (max_base - max_test) / max_base
    # Run-to-run noise of each side, relative to its median: when it is comparable to the
    # threshold, a single comparison cannot tell overhead from noise.
    noise_base = 100.0 * std_base / med_base
    noise_test = 100.0 * std_test / med_test

    print(
        "\nOverhead (%): median "
        + format(diff_med, ".2f")
        + " (PRIMARY)"
        + ", average "
        + format(diff_avg, ".2f")
        + "; same-rank pairs: slowest runs "
        + format(diff_slowest, ".2f")
        + ", fastest runs "
        + format(diff_fastest, ".2f")
    )
    print(
        "Run-to-run noise (std/median, %): baseline "
        + format(noise_base, ".2f")
        + ", test "
        + format(noise_test, ".2f")
    )
    if max(noise_base, noise_test) > threshold_overhead:
        print(
            "WARNING: run-to-run noise exceeds the threshold; the verdict below is not reliable "
            "on this run"
        )

    return config.analyze(threshold_overhead, results, diff_med)


def paired_overheads(baseline, test):
    """Overhead of each measurement pair in %, 100 * (1 - test/baseline).

    Every iteration runs the baseline and then the test back to back, so both see the same
    machine state (CPU/GPU frequency, temperature, background load); the ratio within a pair
    cancels what the two runs share, and only the jitter within a pair remains.
    """
    return [100.0 * (1.0 - t / b) for b, t in zip(baseline, test) if b > 0]


def median_interval(values, confidence=CONFIDENCE):
    """Median and a distribution-free confidence interval for it, from order statistics.

    Each value is below or above the true median with probability 1/2, so the number of
    values below it is Binomial(n, 1/2). The interval [x_(k), x_(n+1-k)] of the sorted
    values misses the true median only when fewer than k values fall on one side, so its
    coverage is 1 - 2 * P(Binomial(n, 1/2) <= k - 1), exactly, without assuming a normal
    distribution and without trimming outliers. The largest k that still reaches the
    requested confidence gives the narrowest such interval (for 15 values: k = 4, 96.5%).
    This is the textbook interval for a quantile, equivalent to
    scipy.stats.quantile_test(values, p=0.5).confidence_interval(confidence); it is
    implemented here with the standard library so that the test needs no extra packages.

    Returns (median, low, high, coverage, k); low/high are None when n is too small for
    any order-statistic interval to reach the confidence (n < 6 for 95%).
    """
    ordered = sorted(values)
    n = len(ordered)
    if n == 0:
        return 0.0, None, None, 0.0, 0
    median = statistics.median(ordered)
    best = None
    tail = 0
    for k in range(1, n // 2 + 1):
        tail += math.comb(n, k - 1)  # P(at most k-1 below) * 2**n
        coverage = 1.0 - 2.0 * tail / 2**n
        if coverage < confidence:
            break
        best = (k, coverage)
    if best is None:
        return median, None, None, 0.0, 0
    k, coverage = best
    return median, ordered[k - 1], ordered[n - k], coverage, k


def paired_verdict(threshold_overhead, baseline, test, confidence=CONFIDENCE):
    """'BELOW', 'ABOVE' or 'UNDECIDED' for the paired overheads against the threshold."""
    _, low, high, _, _ = median_interval(paired_overheads(baseline, test), confidence)
    if low is None:
        return "UNDECIDED"
    if high <= threshold_overhead:
        return "BELOW"
    if low > threshold_overhead:
        return "ABOVE"
    return "UNDECIDED"


def analyze_paired(threshold_overhead, baseline, test, confidence=CONFIDENCE, checks=1):
    """Pass/fail from the paired overheads and their confidence interval; returns the exit code.

    The test passes only when the whole interval is at or below the threshold and fails when
    it is above it. When the threshold lies inside the interval even after the adaptive
    repetitions, the runs cannot tell whether the overhead is above or below it: the result
    is UNDECIDED and the test fails, so that a too-noisy machine is visible instead of a
    verdict that would flip between runs.
    """
    overheads = paired_overheads(baseline, test)
    median, low, high, coverage, k = median_interval(overheads, confidence)
    scope = (
        f" per check, >= {CONFIDENCE:.0%} over all {checks} checks"
        if checks > 1
        else ""
    )
    n = len(overheads)
    print(
        "\nPaired analysis - overhead of each pair, 100 * (1 - test/baseline), sorted (%):"
    )
    print("  " + ", ".join(format(v, ".1f") for v in sorted(overheads)))
    if low is None:
        print(
            f"Paired overhead (%): median {median:.2f} (PRIMARY); {n} pairs are too few for "
            f"a {confidence:.1%} interval"
        )
        print(f"\nTest failed - UNDECIDED: only {n} valid pairs")
        return 1
    print(
        f"Paired overhead (%): median {median:.2f} (PRIMARY), {coverage:.1%} interval{scope} "
        f"[{low:.2f}, {high:.2f}] (sorted pairs {k} and {n + 1 - k} of {n}; distribution-free)"
    )
    if high <= threshold_overhead:
        print(
            f"\nTest passed - Measured overhead {median:.2f}% is confidently below the "
            f"threshold {threshold_overhead}%"
        )
        return 0
    if low > threshold_overhead:
        print(
            f"\nTest failed - Measured overhead {median:.2f}% is confidently above the "
            f"threshold {threshold_overhead}%"
        )
        return 1
    # Why undecided: either the machine is noisy (the baseline itself varies a lot between runs),
    # or it is quiet and the overhead simply lies close to the threshold.
    noise = machine_noise_percent(baseline)
    reason_header = (
        f"\nTest failed - UNDECIDED: the threshold {threshold_overhead}% is inside the "
        f"{coverage:.1%} interval [{low:.2f}, {high:.2f}] after {n} pairs (median {median:.2f}%)"
    )
    if noise <= QUIET_MACHINE_NOISE_PERCENT:
        print(
            f"{reason_header}. The machine was quiet (baseline runs vary by {noise:.1f}%), so "
            f"the measurement is precise, but the overhead is close to the threshold: the median "
            f"is {abs(median - threshold_overhead):.2f} points from it, less than the interval "
            "allows. The overhead is about the size of the threshold."
        )
    else:
        print(
            f"{reason_header}. The machine was too noisy (baseline runs vary by {noise:.1f}%) to "
            "tell whether the overhead is above or below the threshold. A quieter machine or "
            "more pairs would narrow the interval."
        )
    return 1


def machine_noise_percent(baseline):
    """Robust run-to-run variation of the baseline throughput, in % of its median.

    The baseline is the same binary run again and again, so its spread is the machine's noise.
    Uses the median absolute deviation (scaled by 1.4826 to match a standard deviation for normal
    data), so a single slow or fast run does not dominate.
    """
    if len(baseline) < 2:
        return 0.0
    center = statistics.median(baseline)
    mad = statistics.median(abs(b - center) for b in baseline)
    return 100.0 * 1.4826 * mad / center if center else 0.0


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

    qos_fd = None
    if test_configs[test_type].limit_cpu_idle:
        qos_fd = hold_cpu_latency_qos()
    work_dir = tempfile.mkdtemp(prefix="pti_perf_")
    try:
        result, test_results = run_test(
            executable_path, work_dir, test_type, test_timeout, threshold_overhead
        )
    finally:
        if qos_fd is not None:
            os.close(qos_fd)
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
