#!/usr/bin/env python3
"""
Profile Time Difference (prof_time_diff.py)

Test script to validate that kernel durations between two consecutive runs
of an application differ by no more than a specified threshold percentage.

Usage:
    python3 prof_time_diff.py <binary_path> [-dp PERCENT] [-da NANOSECONDS] [-s SKIP]

Arguments:
    binary_path: Path to the target executable

Options:
    -dp, --diff-percent PERCENT   Maximum allowed duration difference in percent
    -da, --diff-abs NANOSECONDS   Maximum allowed duration difference in nanoseconds
    -s, --skip COUNT              Number of first kernels to skip (warmup kernels) (default: 0)
    -e1 VAR=VALUE                 Environment variable(s) for run 1 (can be specified multiple times)
    -e2 VAR=VALUE                 Environment variable(s) for run 2 (can be specified multiple times)

    Note: If neither -dp nor -da is specified, defaults to -dp 20.0
          You can use -dp alone, -da alone, or both together.
          When both are specified, a kernel violates if it exceeds EITHER threshold.

Examples:
    python3 prof_time_diff.py /path/to/application
    python3 prof_time_diff.py /path/to/application -dp 25.0
    python3 prof_time_diff.py /path/to/application -dp 20.0 -s 10
    python3 prof_time_diff.py /path/to/application --diff-percent 15.5 --skip 5
    python3 prof_time_diff.py /path/to/application -da 1000
    python3 prof_time_diff.py /path/to/application --diff-abs 5000 -s 20
    python3 prof_time_diff.py /path/to/application -e1 SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1
    python3 prof_time_diff.py /path/to/application -e1 VAR1=value1 -e1 VAR2=value2 -e2 VAR1=value3
"""

import sys
import subprocess
import re
import os
import argparse
import math
from pathlib import Path
from typing import List, Tuple, Optional


class KernelRecord:
    """Represents a single kernel execution record."""

    def __init__(self, name: str, duration_ns: int, position: int):
        self.name = name
        self.duration_ns = duration_ns
        self.position = position  # Position in execution order

    def __repr__(self):
        return f"KernelRecord(name='{self.name}', duration={self.duration_ns}ns, pos={self.position})"


class Violation:
    """Represents a kernel duration violation."""

    def __init__(
        self,
        position: int,
        name: str,
        duration1_ns: int,
        duration2_ns: int,
        diff_percent: float,
        diff_abs: int,
    ):
        self.position = position
        self.name = name
        self.duration1_ns = duration1_ns
        self.duration2_ns = duration2_ns
        self.diff_percent = diff_percent
        self.diff_abs = diff_abs

    def __repr__(self):
        return (
            f"Violation at position {self.position}: {self.name}\n"
            f"  Run1: {self.duration1_ns:,}ns, Run2: {self.duration2_ns:,}ns, "
            f"Diff: {self.diff_percent:.2f}% ({self.diff_abs:,}ns)"
        )


def parse_kernel_records(output: str) -> List[KernelRecord]:
    """
    Parse PTI kernel records from application output.

    Expected format:
        Found Kernel Record
        Kernel Name: <name>
        Kernel Execution Time: <duration> ns
        ...

    Returns:
        List of KernelRecord objects in order of appearance.
    """
    records = []
    lines = output.split("\n")
    i = 0
    position = 0

    while i < len(lines):
        line = lines[i].strip()

        # Look for kernel record marker
        if line == "Found Kernel Record":
            kernel_name = None
            kernel_duration = None
            j = i  # Initialize j in case the for loop doesn't execute

            # Parse the next few lines for kernel name and duration
            for j in range(i + 1, min(i + 20, len(lines))):
                curr_line = lines[j].strip()

                # Extract kernel name
                name_match = re.match(r"Kernel Name:\s*(.+)$", curr_line)
                if name_match:
                    kernel_name = name_match.group(1).strip()

                # Extract duration (format: "Kernel Execution Time: 1'234'567 ns")
                duration_match = re.match(
                    r"Kernel Execution Time:\s*([\d']+)\s*ns", curr_line
                )
                if duration_match:
                    # Remove apostrophes (thousands separator) and convert to int
                    duration_str = duration_match.group(1).replace("'", "")
                    kernel_duration = int(duration_str)

                # If we have both, create record and move on
                if kernel_name and kernel_duration is not None:
                    records.append(KernelRecord(kernel_name, kernel_duration, position))
                    position += 1
                    break

            # Move past the processed lines (or just past "Found Kernel Record" if incomplete)
            i = j + 1
        else:
            i += 1

    return records


def run_target_binary(
    binary_path: str, output_file: str, env_vars: Optional[dict] = None
) -> Tuple[int, str]:
    """
    Run target binary and capture output.

    Args:
        binary_path: Path to the target executable
        output_file: File to save the output
        env_vars: Optional dictionary of environment variables to set for this run

    Returns:
        (return_code, output_text)
    """
    binary_name = os.path.basename(binary_path)
    try:
        # Prepare environment
        env = os.environ.copy()
        if env_vars:
            env.update(env_vars)

        # Redirect stderr to stdout to preserve chronological order of records
        # (PTI records may be emitted to both streams, and order matters for comparison)
        result = subprocess.run(
            [binary_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=300,  # 5 minute timeout
            env=env,
        )

        # All output is now in stdout with correct chronological order
        full_output = result.stdout

        # Save to file
        with open(output_file, "w") as f:
            f.write(full_output)

        return result.returncode, full_output

    except subprocess.TimeoutExpired:
        print(
            f"ERROR: Target binary '{binary_name}' execution timed out after 300 seconds",
            file=sys.stderr,
        )
        return -1, ""
    except Exception as e:
        print(
            f"ERROR: Failed to run target binary '{binary_name}': {e}", file=sys.stderr
        )
        return -1, ""


def compare_kernel_durations(
    run1_records: List[KernelRecord],
    run2_records: List[KernelRecord],
    threshold_percent: Optional[float] = None,
    threshold_abs: Optional[int] = None,
    skip_first: int = 0,
) -> Tuple[List[Violation], List[str]]:
    """
    Compare kernel durations between two runs.

    Args:
        run1_records: Kernel records from first run
        run2_records: Kernel records from second run
        threshold_percent: Maximum allowed duration difference in percent (optional)
        threshold_abs: Maximum allowed duration difference in nanoseconds (optional)
        skip_first: Number of first kernels to skip (warmup)

    Returns:
        (violations, errors)
        - violations: List of kernels exceeding threshold
        - errors: List of error messages (mismatched names, counts, etc.)
    """
    violations = []
    errors = []

    # Check if kernel counts match
    if len(run1_records) != len(run2_records):
        errors.append(
            f"Kernel count mismatch: Run1={len(run1_records)}, Run2={len(run2_records)}"
        )
        # Continue comparison up to the shorter list

    # Compare kernels sequentially, starting after skip_first
    min_count = min(len(run1_records), len(run2_records))

    # Check if skip_first is valid
    if skip_first >= min_count:
        errors.append(
            f"skip_first ({skip_first}) is >= total kernels ({min_count}), no kernels to compare"
        )
        return violations, errors

    for i in range(skip_first, min_count):
        kernel1 = run1_records[i]
        kernel2 = run2_records[i]

        # Check if kernel names match
        if kernel1.name != kernel2.name:
            errors.append(
                f"Position {i}: Kernel name mismatch - Run1='{kernel1.name}', Run2='{kernel2.name}'"
            )
            continue

        # Calculate differences
        if kernel1.duration_ns == 0:
            errors.append(f"Position {i}: {kernel1.name} has zero duration in Run1")
            continue

        diff_ns = abs(kernel2.duration_ns - kernel1.duration_ns)
        diff_percent = (diff_ns / kernel1.duration_ns) * 100.0

        # Check if exceeds threshold(s)
        violates = False
        if threshold_percent is not None and diff_percent > threshold_percent:
            violates = True
        if threshold_abs is not None and diff_ns > threshold_abs:
            violates = True

        if violates:
            violations.append(
                Violation(
                    position=i,
                    name=kernel1.name,
                    duration1_ns=kernel1.duration_ns,
                    duration2_ns=kernel2.duration_ns,
                    diff_percent=diff_percent,
                    diff_abs=diff_ns,
                )
            )

    return violations, errors


def validate_threshold(value: Optional[float], name: str) -> bool:
    """
    Validate a threshold value.

    Args:
        value: The threshold value to validate (can be None)
        name: Human-readable name for error messages (e.g., "Percentage threshold")

    Returns:
        True if valid or None, False if invalid (error message printed to stderr)
    """
    if value is None:
        return True

    if not math.isfinite(value):
        print(
            f"ERROR: {name} must be a finite number (not NaN or infinity)",
            file=sys.stderr,
        )
        return False

    if value <= 0:
        print(f"ERROR: {name} must be a positive number", file=sys.stderr)
        return False

    return True


def parse_env_vars(env_list: Optional[List[str]], run_name: str) -> dict:
    """
    Parse environment variable specifications from command line.

    Args:
        env_list: List of environment variable specifications in VAR=VALUE format
        run_name: Name of the run (for error messages, e.g., "run 1")

    Returns:
        Dictionary of environment variables

    Raises:
        SystemExit: If any environment variable specification is invalid
    """
    env_vars = {}
    if env_list:
        for env_spec in env_list:
            if "=" not in env_spec:
                print(
                    f"ERROR: Invalid environment variable format for {run_name}: '{env_spec}'. Must be VAR=VALUE",
                    file=sys.stderr,
                )
                sys.exit(1)
            key, value = env_spec.split("=", 1)
            if not key:
                print(
                    f"ERROR: Invalid environment variable format for {run_name}: '{env_spec}'. Variable name cannot be empty",
                    file=sys.stderr,
                )
                sys.exit(1)
            env_vars[key] = value
    return env_vars


def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(
        description="Test script to validate that kernel durations between two consecutive runs "
        "of an application differ by no more than a specified threshold percentage "
        "or/and absolute value of nanoseconds.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s /path/to/application
  %(prog)s /path/to/application -dp 25.0
  %(prog)s /path/to/application -dp 20.0 -s 10
  %(prog)s /path/to/application --diff-percent 15.5 --skip 5
  %(prog)s /path/to/application -da 1000
  %(prog)s /path/to/application --diff-abs 5000 -s 20
  %(prog)s /path/to/application -s 50 -dp 30
  %(prog)s /path/to/application -e1 SYCL_DEVICE_FILTER=level_zero:gpu
  %(prog)s /path/to/application -e1 VAR1=value1 -e2 VAR1=value2
  %(prog)s /path/to/application -e1 A=1 -e1 B=2 -e2 A=3 -e2 B=4
        """,
    )

    parser.add_argument(
        "binary", metavar="BINARY_PATH", help="Path to the target executable"
    )

    parser.add_argument(
        "-dp",
        "--diff-percent",
        type=float,
        metavar="PERCENT",
        dest="diff_percent",
        help="Maximum allowed duration difference in percent",
    )

    parser.add_argument(
        "-da",
        "--diff-abs",
        type=int,
        metavar="NANOSECONDS",
        dest="diff_abs",
        help="Maximum allowed duration difference in nanoseconds",
    )

    parser.add_argument(
        "-s",
        "--skip",
        type=int,
        default=0,
        metavar="COUNT",
        help="Number of first kernels to skip (warmup kernels) (default: 0)",
    )

    parser.add_argument(
        "-e1",
        action="append",
        metavar="VAR=VALUE",
        dest="env1",
        help="Environment variable for run 1 in format VAR=VALUE (can be specified multiple times)",
    )

    parser.add_argument(
        "-e2",
        action="append",
        metavar="VAR=VALUE",
        dest="env2",
        help="Environment variable for run 2 in format VAR=VALUE (can be specified multiple times)",
    )

    # Parse arguments
    args = parser.parse_args()

    binary_path = args.binary
    threshold_percent = args.diff_percent
    threshold_abs = args.diff_abs
    skip_first = args.skip

    # If neither threshold is specified, default to 20% for backwards compatibility
    if threshold_percent is None and threshold_abs is None:
        threshold_percent = 20.0

    # Parse environment variables
    env1_vars = parse_env_vars(args.env1, "run 1")
    env2_vars = parse_env_vars(args.env2, "run 2")

    # Validate thresholds
    if not validate_threshold(threshold_percent, "Percentage threshold"):
        return 1

    if not validate_threshold(threshold_abs, "Absolute threshold"):
        return 1

    # Validate skip_first
    if skip_first < 0:
        print("ERROR: skip_first must be a non-negative integer", file=sys.stderr)
        return 1

    # Validate binary exists
    if not os.path.isfile(binary_path):
        print(f"ERROR: Binary not found: {binary_path}", file=sys.stderr)
        return 1

    if not os.access(binary_path, os.X_OK):
        print(f"ERROR: Binary is not executable: {binary_path}", file=sys.stderr)
        return 1

    print("=" * 80)
    print("Profile Time Difference Test (prof_time_diff)")
    print("=" * 80)
    print(f"Binary: {binary_path}")

    # Display threshold(s)
    thresholds_str = []
    if threshold_percent is not None:
        thresholds_str.append(f"{threshold_percent}% (percent)")
    if threshold_abs is not None:
        thresholds_str.append(f"{threshold_abs:,} ns (absolute)")
    print(f"Threshold: {', '.join(thresholds_str)}")

    print(f"Skip first: {skip_first} kernel(s)")
    if env1_vars:
        print(f"Run 1 env vars: {', '.join(f'{k}={v}' for k, v in env1_vars.items())}")
    if env2_vars:
        print(f"Run 2 env vars: {', '.join(f'{k}={v}' for k, v in env2_vars.items())}")
    print()

    # Create output directory for intermediate files based on executable name
    binary_name = os.path.basename(binary_path)
    output_dir = Path(f"{binary_name}_test_output")
    output_dir.mkdir(exist_ok=True)

    run1_file = output_dir / "run1_output.txt"
    run2_file = output_dir / "run2_output.txt"

    # Run binary twice
    print(f"Running {binary_name} (Run 1)...")
    ret1, output1 = run_target_binary(
        binary_path, str(run1_file), env_vars=env1_vars if env1_vars else None
    )
    if ret1 != 0:
        print(f"ERROR: Run 1 failed with return code {ret1}", file=sys.stderr)
        print(f"Output saved to: {run1_file}")
        return 1
    print(f"  Completed. Output saved to: {run1_file}")

    print(f"\nRunning {binary_name} (Run 2)...")
    ret2, output2 = run_target_binary(
        binary_path, str(run2_file), env_vars=env2_vars if env2_vars else None
    )
    if ret2 != 0:
        print(f"ERROR: Run 2 failed with return code {ret2}", file=sys.stderr)
        print(f"Output saved to: {run2_file}")
        return 1
    print(f"  Completed. Output saved to: {run2_file}")

    # Parse kernel records
    print("\nParsing kernel records...")
    run1_records = parse_kernel_records(output1)
    run2_records = parse_kernel_records(output2)

    print(f"  Run 1: Found {len(run1_records)} kernel executions")
    print(f"  Run 2: Found {len(run2_records)} kernel executions")

    if len(run1_records) == 0 or len(run2_records) == 0:
        print("\nERROR: No kernel records found in one or both runs.", file=sys.stderr)
        print("This may indicate:")
        print("  - PTI tracing is not enabled")
        print(f"  - {binary_name} is not executing GPU kernels")
        print("  - Output parsing failed")
        return 1

    # Check for zero duration kernels
    print("\nValidating kernel durations...")
    zero_duration_kernels = []

    for i, kernel in enumerate(run1_records):
        if kernel.duration_ns == 0:
            zero_duration_kernels.append(f"Run 1, Position {i}: {kernel.name}")

    for i, kernel in enumerate(run2_records):
        if kernel.duration_ns == 0:
            zero_duration_kernels.append(f"Run 2, Position {i}: {kernel.name}")

    if zero_duration_kernels:
        print(
            f"\nERROR: Found {len(zero_duration_kernels)} kernel(s) with zero duration:",
            file=sys.stderr,
        )
        for kernel_info in zero_duration_kernels:
            print(f"  - {kernel_info}", file=sys.stderr)
        print("\nThis indicates a timing or instrumentation issue.", file=sys.stderr)
        return 1

    print("  All kernels have non-zero durations")

    # Validate skip_first doesn't exceed total kernels
    min_count = min(len(run1_records), len(run2_records))
    if skip_first > 0:
        if skip_first >= min_count:
            print(
                f"\nERROR: skip_first ({skip_first}) is >= total kernels ({min_count})",
                file=sys.stderr,
            )
            return 1
        print(f"  Skipping first {skip_first} kernel(s) (warmup)")
        print(f"  Comparing {min_count - skip_first} kernel(s)")

    # Compare kernel durations
    print("\nComparing kernel durations...")
    violations, errors = compare_kernel_durations(
        run1_records,
        run2_records,
        threshold_percent=threshold_percent,
        threshold_abs=threshold_abs,
        skip_first=skip_first,
    )

    # Report results
    print("\n" + "=" * 80)
    print("RESULTS")
    print("=" * 80)

    # Report errors first
    if errors:
        print("\nERRORS:")
        for error in errors:
            print(f"  - {error}")

    # Report violations
    if violations:
        # Build violation header based on active thresholds
        threshold_desc = []
        if threshold_percent is not None:
            threshold_desc.append(f"{threshold_percent}%")
        if threshold_abs is not None:
            threshold_desc.append(f"{threshold_abs:,}ns")
        threshold_str = " or ".join(threshold_desc)

        print(
            f"\nVIOLATIONS: {len(violations)} kernel(s) exceeded threshold ({threshold_str}):"
        )
        print()
        for v in violations:
            print(f"  Position {v.position}: {v.name}")
            print(f"    Run1: {v.duration1_ns:>15,} ns")
            print(f"    Run2: {v.duration2_ns:>15,} ns")
            print(f"    Diff: {v.diff_percent:>15.2f}% ({v.diff_abs:,} ns)")
            print()
    else:
        # Build success message based on active thresholds
        threshold_desc = []
        if threshold_percent is not None:
            threshold_desc.append(f"{threshold_percent}%")
        if threshold_abs is not None:
            threshold_desc.append(f"{threshold_abs:,}ns")
        threshold_str = " and ".join(threshold_desc)
        print(
            f"\nNo violations found. All kernels are within threshold ({threshold_str})."
        )

    # Summary
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    total_kernels = min(len(run1_records), len(run2_records))
    kernels_compared = total_kernels - skip_first
    if skip_first > 0:
        print(f"Total kernels found: {total_kernels}")
        print(f"Kernels skipped: {skip_first}")
        print(f"Kernels compared: {kernels_compared}")
    else:
        print(f"Total kernels compared: {kernels_compared}")

    # Print comparison criteria
    criteria_parts = []
    if threshold_percent is not None:
        criteria_parts.append(f"Percentage difference {threshold_percent}%")
    if threshold_abs is not None:
        criteria_parts.append(f"Absolute difference {threshold_abs:,} ns")
    criteria_str = " OR ".join(criteria_parts)
    print(f"Comparison criteria: {criteria_str}")

    # Build violation summary based on active thresholds
    threshold_desc = []
    if threshold_percent is not None:
        threshold_desc.append(f">{threshold_percent}%")
    if threshold_abs is not None:
        threshold_desc.append(f">{threshold_abs:,}ns")
    threshold_str = " or ".join(threshold_desc)
    print(f"Violations ({threshold_str}): {len(violations)}")

    # Categorize errors by type for summary
    if errors:
        error_counts = {
            "count_mismatch": 0,
            "name_mismatch": 0,
            "zero_duration": 0,
            "skip_first": 0,
        }
        for err in errors:
            if "Kernel count mismatch" in err:
                error_counts["count_mismatch"] += 1
            elif "Kernel name mismatch" in err:
                error_counts["name_mismatch"] += 1
            elif "zero duration" in err:
                error_counts["zero_duration"] += 1
            elif "skip_first" in err:
                error_counts["skip_first"] += 1

        # Build error summary string
        error_types = []
        if error_counts["count_mismatch"] > 0:
            error_types.append("count mismatch")
        if error_counts["name_mismatch"] > 0:
            error_types.append(f"{error_counts['name_mismatch']} name mismatch")
        if error_counts["zero_duration"] > 0:
            error_types.append(f"{error_counts['zero_duration']} zero duration")
        if error_counts["skip_first"] > 0:
            error_types.append("invalid skip_first")

        error_detail = ", ".join(error_types) if error_types else ""
        print(f"Errors: {len(errors)} ({error_detail})")
    else:
        print(f"Errors: 0 (checked: count, name, zero duration)")

    # Report biggest violation
    if violations:
        max_violation = max(violations, key=lambda v: v.diff_percent)
        print(f"\nBiggest violation:")
        print(f"  Position: {max_violation.position}")
        print(f"  Kernel: {max_violation.name}")
        print(f"  Run 1: {max_violation.duration1_ns:,} ns")
        print(f"  Run 2: {max_violation.duration2_ns:,} ns")
        print(
            f"  Difference: {max_violation.diff_percent:.2f}% ({max_violation.diff_abs:,} ns)"
        )

    # Determine exit code
    if errors or violations:
        print("\nStatus: FAIL")
        return 1
    else:
        print("\nStatus: PASS")
        return 0


if __name__ == "__main__":
    sys.exit(main())
