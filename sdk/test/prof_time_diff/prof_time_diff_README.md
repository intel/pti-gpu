# Profile Time Difference Test (prof_time_diff)

## Overview

This test validates that GPU kernel execution durations remain consistent between consecutive runs of a target application. It captures kernel timing from PTI records and ensures variations don't exceed specified thresholds.

**Use cases:**
- Detect performance regressions or unexpected timing changes
- Validate PTI tracing accuracy
- Compare performance under different runtime configurations (via environment variables)
- Test kernel execution stability

## Quick Start

```bash
# Basic usage with default 20% threshold
python3 prof_time_diff.py /path/to/application

# With absolute threshold (3000 nanoseconds)
python3 prof_time_diff.py /path/to/application -da 3000

# Skip first 50 kernels (warmup) and use percentage threshold
python3 prof_time_diff.py /path/to/application -dp 20 -s 50

# Compare with different environment variables
python3 prof_time_diff.py /path/to/application -e1 PTI_DEVICE_SYNC_DELTA=50000 -e2 PTI_DEVICE_SYNC_DELTA=100000
```

## Threshold Types

- **`-dp, --diff-percent`**: Percentage difference (e.g., `-dp 20` = 20% variation allowed)
- **`-da, --diff-abs`**: Absolute difference in nanoseconds (e.g., `-da 3000` = 3μs variation allowed)
- **Both**: Can use both together - violation if EITHER threshold is exceeded
- **Default**: If neither specified, defaults to `-dp 20.0`

## Common Options

- `-s, --skip COUNT`: Skip first N kernels (warmup/initialization)
- `-e1 VAR=VALUE`: Environment variable for first run (repeatable)
- `-e2 VAR=VALUE`: Environment variable for second run (repeatable)

## Exit Codes

- **0**: PASS - All kernel durations within threshold
- **1**: FAIL - Violations found or errors detected

## How It Works

1. Runs the target application twice
2. Captures PTI kernel records from both runs (stderr redirected to stdout to preserve chronological order)
3. Validates all kernels have non-zero durations
4. Compares corresponding kernels by position (after optional warmup skip)
5. Reports violations and biggest outlier

## Output

The test creates a `<binary_name>_test_output/` directory with:
- `run1_output.txt`: Full output from first run
- `run2_output.txt`: Full output from second run

Results are printed to console showing:
- Number of kernels compared
- Violations exceeding threshold
- Biggest violation with both run durations
- Pass/Fail status

## Full Documentation

For complete documentation including all options, examples, and troubleshooting:

```bash
python3 prof_time_diff.py --help
```

## CTest Integration

The test is integrated with CTest as `prof-time-diff-dlworkload`:

```bash
# From build directory
ctest -R prof-time-diff-dlworkload -V
```
