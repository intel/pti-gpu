#!/usr/bin/env python3

# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import subprocess
import tempfile
from pathlib import Path

# Test configuration constants
REPETITIONS = 5  # Number of repetitions for the test workload (-r parameter)
TARGET_EXECUTABLE = "dpc_gemm_threaded_prof_print"  # The specific executable, instrumented with XPTI, to use in the test

def check_environment_preconditions():
    """
    Check that the environment is properly configured for the XPTI interoperability test.

    Returns:
        bool: True if environment is clean, False if preconditions are not met
    """
    if 'XPTI_SUBSCRIBERS' in os.environ:
        print("ERROR: XPTI_SUBSCRIBERS environment variable is already set!")
        print(f"   Current value: {os.environ['XPTI_SUBSCRIBERS']}")
        print("   This will interfere with the test. Please unset it and run again.")
        print("   Use: unset XPTI_SUBSCRIBERS")
        return False

    return True

def run_executable_with_env(executable_path, env_vars=None, timeout=60):
    """
    Run an executable with specific environment variables and capture output.

    Args:
        executable_path: Path to the executable
        env_vars: Dictionary of environment variables to set
        timeout: Timeout in seconds

    Returns:
        tuple: (return_code, stdout, stderr)
    """
    # Start with current environment
    env = os.environ.copy()

    # Add/override with provided environment variables
    if env_vars:
        env.update(env_vars)

    # Construct command with specific arguments: 1 thread, size 64, configurable repetitions
    command = [executable_path, "-t", "1", "-s", "64", "-r", str(REPETITIONS), "-c", "gpu", "-c", "-sycl"]

    print(f"Running: {' '.join(command)}")
    if env_vars:
        print(f"Environment: {env_vars}")

    try:
        result = subprocess.run(
            command,
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        print(f"ERROR: Executable timed out after {timeout} seconds")
        return -1, "", "Timeout"
    except Exception as e:
        print(f"ERROR: Failed to run executable: {e}")
        return -1, "", str(e)

def analyze_output_differences(output1, output2, label1, label2):
    """
    Analyze differences between two outputs.

    Args:
        output1: First output string
        output2: Second output string
        label1: Label for first output
        label2: Label for second output

    Returns:
        dict: Analysis results
    """
    lines1 = output1.strip().split('\n') if output1 else []
    lines2 = output2.strip().split('\n') if output2 else []

    analysis = {
        'line_count_1': len(lines1),
        'line_count_2': len(lines2),
        'identical': output1 == output2,
        'empty_1': len(output1.strip()) == 0,
        'empty_2': len(output2.strip()) == 0,
        'differences': []
    }

    # Check for XPTI subscriber messages
    xpti_patterns = ['XPTI', 'subscriber', 'unitrace']
    analysis['xpti_found_1'] = any(pattern in output1 for pattern in xpti_patterns)
    analysis['xpti_found_2'] = any(pattern in output2 for pattern in xpti_patterns)

    # Check for warning messages
    analysis['warning_found_1'] = 'warning' in output1.lower()
    analysis['warning_found_2'] = 'warning' in output2.lower()

    # Check specifically for Sycl Runtime Records
    sycl_runtime_pattern = 'Found Sycl Runtime Record'
    analysis['sycl_runtime_found_1'] = sycl_runtime_pattern in output1
    analysis['sycl_runtime_found_2'] = sycl_runtime_pattern in output2

    # Count different record types in both outputs
    record_types = [
        'Found Sycl Runtime Record',
        'Found Kernel Record',
        'Found Memory Record',
        'Found Invalid Record'
    ]

    analysis['record_counts_1'] = {}
    analysis['record_counts_2'] = {}

    for record_type in record_types:
        analysis['record_counts_1'][record_type] = output1.count(record_type)
        analysis['record_counts_2'][record_type] = output2.count(record_type)

    if not analysis['identical']:
        # Find line differences
        max_lines = max(len(lines1), len(lines2))
        for i in range(max_lines):
            line1 = lines1[i] if i < len(lines1) else ""
            line2 = lines2[i] if i < len(lines2) else ""
            if line1 != line2:
                analysis['differences'].append({
                    'line': i + 1,
                    label1: line1,
                    label2: line2
                })

    return analysis

def run_test_scenarios(executable_path):
    """
    Execute both test scenarios: clean run and foreign subscriber run.

    Args:
        executable_path: Path to the executable to test

    Returns:
        tuple: (ret1, stdout1, stderr1, ret2, stdout2, stderr2)
    """
    # Test 1: Run without any XPTI_SUBSCRIBERS (clean PTI run)
    print("\n1. Running without XPTI_SUBSCRIBERS (clean PTI run)...")
    ret1, stdout1, stderr1 = run_executable_with_env(executable_path)

    # Test 2: Run with XPTI_SUBSCRIBERS=fake_subscr (foreign subscriber)
    print("\n2. Running with XPTI_SUBSCRIBERS=fake_subscr (foreign subscriber)...")
    env_vars = {"XPTI_SUBSCRIBERS": "fake_subscr"}
    ret2, stdout2, stderr2 = run_executable_with_env(executable_path, env_vars)

    return ret1, stdout1, stderr1, ret2, stdout2, stderr2

def analyze_outputs_and_report_statistics(ret1, stdout1, stderr1, ret2, stdout2, stderr2):
    """
    Analyze outputs and generate detailed reports.

    Args:
        ret1, stdout1, stderr1: Results from clean run
        ret2, stdout2, stderr2: Results from foreign subscriber run

    Returns:
        tuple: (stdout_analysis, stderr_analysis)
    """
    print("\n" + "=" * 60)
    print("RESULTS ANALYSIS")
    print("=" * 60)

    # Analyze return codes
    print(f"\nReturn codes:")
    print(f"  Clean run: {ret1}")
    print(f"  With foreign subscriber: {ret2}")

    # Analyze stdout
    print(f"\nSTDOUT Analysis:")
    stdout_analysis = analyze_output_differences(stdout1, stdout2, "clean", "foreign_subscriber")
    print(f"  Clean run output length: {stdout_analysis['line_count_1']} lines")
    print(f"  Foreign subscriber output length: {stdout_analysis['line_count_2']} lines")
    print(f"  Outputs identical: {stdout_analysis['identical']}")
    print(f"  Sycl Runtime Records found (clean): {stdout_analysis['sycl_runtime_found_1']}")
    print(f"  Sycl Runtime Records found (foreign): {stdout_analysis['sycl_runtime_found_2']}")
    print(f"  Warning found (clean): {stdout_analysis['warning_found_1']}")
    print(f"  Warning found (foreign): {stdout_analysis['warning_found_2']}")

    # Print record counts for detailed analysis
    print(f"\nRecord Type Counts:")
    for record_type in stdout_analysis['record_counts_1'].keys():
        count1 = stdout_analysis['record_counts_1'][record_type]
        count2 = stdout_analysis['record_counts_2'][record_type]
        print(f"  {record_type}: Clean={count1}, Foreign={count2}")

    # Analyze stderr
    print(f"\nSTDERR Analysis:")
    stderr_analysis = analyze_output_differences(stderr1, stderr2, "clean", "foreign_subscriber")
    print(f"  Clean run stderr length: {stderr_analysis['line_count_1']} lines")
    print(f"  Foreign subscriber stderr length: {stderr_analysis['line_count_2']} lines")
    print(f"  Stderr identical: {stderr_analysis['identical']}")

    return stdout_analysis, stderr_analysis

def perform_behavioral_checks(ret1, ret2, stdout_analysis, stderr_analysis):
    """
    Perform all behavioral validation checks.

    Args:
        ret1, ret2: Return codes from both runs
        stdout_analysis: Analysis results from stdout comparison
        stderr_analysis: Analysis results from stderr comparison

    Returns:
        bool: True if all checks pass
    """
    print(f"\n" + "=" * 60)
    print("EXPECTED BEHAVIOR VERIFICATION")
    print("=" * 60)

    test_passed = True

    # Check 1: Both runs should succeed
    if ret1 != 0 or ret2 != 0:
        print("FAIL: One or both runs failed")
        test_passed = False
    else:
        print("PASS: Both runs completed successfully")

    # Check 2: Clean run should have Sycl Runtime Records
    if stdout_analysis['sycl_runtime_found_1']:
        print("PASS: Found Sycl Runtime Record found in clean run")
    else:
        print("FAIL: No Found Sycl Runtime Record found in clean run")
        test_passed = False

    # Check 3: Foreign subscriber run should have fewer Sycl Runtime Records than clean run
    sycl_count_clean = stdout_analysis['record_counts_1']['Found Sycl Runtime Record']
    sycl_count_foreign = stdout_analysis['record_counts_2']['Found Sycl Runtime Record']
    kernel_count_foreign = stdout_analysis['record_counts_2']['Found Kernel Record']

    if sycl_count_foreign < sycl_count_clean and sycl_count_foreign > 0:
        print(f"PASS: Found Sycl Runtime Record count reduced in foreign run ({sycl_count_clean} -> {sycl_count_foreign})")
    else:
        print(f"FAIL: Expected fewer Found Sycl Runtime Record in foreign run - Clean:{sycl_count_clean}, Foreign:{sycl_count_foreign}")
        test_passed = False

    # Check 4: Foreign subscriber Sycl Runtime Record count should equal Kernel Record count
    if sycl_count_foreign == kernel_count_foreign:
        print(f"PASS: Found Sycl Runtime Record count equals Found Kernel Record count in foreign run ({sycl_count_foreign})")
    else:
        print(f"FAIL: Found Sycl Runtime Record count ({sycl_count_foreign}) != Found Kernel Record count ({kernel_count_foreign}) in foreign run")
        test_passed = False

    # Check 5: All other record types should be identical
    other_records_identical = True
    for record_type in stdout_analysis['record_counts_1'].keys():
        if record_type not in ['Found Sycl Runtime Record']:  # Don't check Sycl Runtime Records as we expect them to differ
            count1 = stdout_analysis['record_counts_1'][record_type]
            count2 = stdout_analysis['record_counts_2'][record_type]
            if count1 != count2:
                print(f"FAIL: {record_type} counts differ - Clean:{count1}, Foreign:{count2}")
                other_records_identical = False
                test_passed = False

    if other_records_identical:
        print("PASS: All non-Sycl Runtime Record types have identical counts")

    # Check 6: Both runs should have kernel records equal to repetitions
    kernel_count_clean = stdout_analysis['record_counts_1']['Found Kernel Record']
    kernel_count_foreign = stdout_analysis['record_counts_2']['Found Kernel Record']

    if kernel_count_clean == REPETITIONS:
        print(f"PASS: Clean run has correct number of kernel records ({kernel_count_clean} == {REPETITIONS})")
    else:
        print(f"FAIL: Clean run kernel records mismatch - Expected:{REPETITIONS}, Got:{kernel_count_clean}")
        test_passed = False

    if kernel_count_foreign == REPETITIONS:
        print(f"PASS: Foreign run has correct number of kernel records ({kernel_count_foreign} == {REPETITIONS})")
    else:
        print(f"FAIL: Foreign run kernel records mismatch - Expected:{REPETITIONS}, Got:{kernel_count_foreign}")
        test_passed = False

    # Check 7: Foreign subscriber run should have a warning message
    if stdout_analysis['warning_found_2']:
        print("PASS: Warning message found in foreign subscriber run")
    else:
        print("FAIL: No warning message found in foreign subscriber run")
        test_passed = False

    # Check 8: Summary of expected behavior
    if sycl_count_clean > sycl_count_foreign > 0 and sycl_count_foreign == kernel_count_foreign:
        print(f"PASS: PTI correctly detected foreign subscriber - reduced SYCL runtime tracing ({sycl_count_clean} -> {sycl_count_foreign})")
    else:
        print(f"FAIL: Unexpected interoperability behavior pattern")
        test_passed = False

    # Show detailed differences if requested
    if stdout_analysis['differences']:
        print(f"\nFirst 5 STDOUT differences:")
        for i, diff in enumerate(stdout_analysis['differences'][:5]):
            print(f"  Line {diff['line']}:")
            print(f"    Clean: '{diff['clean']}'")
            print(f"    Foreign: '{diff['foreign_subscriber']}'")

    if stderr_analysis['differences']:
        print(f"\nFirst 5 STDERR differences:")
        for i, diff in enumerate(stderr_analysis['differences'][:5]):
            print(f"  Line {diff['line']}:")
            print(f"    Clean: '{diff['clean']}'")
            print(f"    Foreign: '{diff['foreign_subscriber']}'")

    return test_passed

def save_detailed_outputs(stdout1, stdout2, stderr1, stderr2):
    """
    Save detailed outputs to temporary files for further analysis.

    Args:
        stdout1: Clean run stdout
        stdout2: Foreign subscriber stdout
        stderr1: Clean run stderr
        stderr2: Foreign subscriber stderr
    """
    # Save detailed stdout for further analysis
    with tempfile.NamedTemporaryFile(mode='w', suffix='_clean_stdout.txt', delete=False) as f:
        f.write(stdout1)
        print(f"\nDetailed clean run stdout saved to: {f.name}")

    with tempfile.NamedTemporaryFile(mode='w', suffix='_foreign_stdout.txt', delete=False) as f:
        f.write(stdout2)
        print(f"Detailed foreign subscriber stdout saved to: {f.name}")

    # Save detailed stderr for further analysis
    with tempfile.NamedTemporaryFile(mode='w', suffix='_clean_stderr.txt', delete=False) as f:
        f.write(stderr1)
        print(f"Detailed clean run stderr saved to: {f.name}")

    with tempfile.NamedTemporaryFile(mode='w', suffix='_foreign_stderr.txt', delete=False) as f:
        f.write(stderr2)
        print(f"Detailed foreign subscriber stderr saved to: {f.name}")

def validate_test_results(ret1, stdout1, stderr1, ret2, stdout2, stderr2):
    """
    Validate the test results and perform all behavioral checks.

    Args:
        ret1, stdout1, stderr1: Results from clean run
        ret2, stdout2, stderr2: Results from foreign subscriber run

    Returns:
        bool: True if all validations pass
    """
    # Analyze and report outputs
    stdout_analysis, stderr_analysis = analyze_outputs_and_report_statistics(ret1, stdout1, stderr1, ret2, stdout2, stderr2)

    # Perform behavioral validation checks
    test_passed = perform_behavioral_checks(ret1, ret2, stdout_analysis, stderr_analysis)

    # Save detailed outputs for further analysis
    save_detailed_outputs(stdout1, stdout2, stderr1, stderr2)

    return test_passed

def test_xpti_interoperability(executable_dir):
    """
    Test PTI XPTI interoperability with XPTI subscribers.

    Args:
        executable_dir: Directory containing the dpc_gemm_threaded_prof_print executable

    Returns:
        bool: Test passed
    """
    executable_path = Path(executable_dir) / TARGET_EXECUTABLE
    executable_name = executable_path.name
    print("=" * 60)
    print(f"PTI XPTI INTEROPERABILITY TEST: {executable_name}")
    print(f"Executable: {executable_path}")
    print("=" * 60)

    # Check environment preconditions
    if not check_environment_preconditions():
        print("=" * 60)
        return False

    # Execute test scenarios
    ret1, stdout1, stderr1, ret2, stdout2, stderr2 = run_test_scenarios(str(executable_path))

    # Validate results
    test_passed = validate_test_results(ret1, stdout1, stderr1, ret2, stdout2, stderr2)

    print(f"\n" + "=" * 60)
    print(f"FINAL RESULT: {'PASS' if test_passed else 'FAIL'}")
    print("=" * 60)

    return test_passed

def main():
    if len(sys.argv) != 2:
        print(f"Usage: interop_test.py <executable_directory>")
        print(f"Example: interop_test.py /path/to/build/tests")
        print(f"  The test will look for '{TARGET_EXECUTABLE}' in the provided directory.")
        return 1

    executable_dir = Path(sys.argv[1])

    if not executable_dir.exists():
        print(f"ERROR: Directory not found: {executable_dir}")
        return 1

    if not executable_dir.is_dir():
        print(f"ERROR: Path is not a directory: {executable_dir}")
        return 1

    try:
        success = test_xpti_interoperability(str(executable_dir))
        return 0 if success else 1
    except Exception as e:
        print(f"ERROR: Test failed with exception: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())
