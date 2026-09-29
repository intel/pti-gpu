# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import subprocess
import shutil
import argparse
import sys
import platform
import fnmatch

def clean_up_testing_folder(test_dir, test_name=None):
    # Construct the path to the 'Testing' folder
    testing_folder = os.path.join(test_dir, 'build', 'Testing')
    result_folder = os.path.join(test_dir, 'build', 'results')
    
    cleanup_status = True
    
    if test_name is not None:
        # Clean up only the specific test folders
        test_result_folder = os.path.join(test_dir, 'build', 'results', test_name)

        # Remove this test's per-scenario Testing/Temporary_<name>* log folders
        if os.path.exists(testing_folder):
            for item in os.listdir(testing_folder):
                if item.startswith(f"Temporary_{test_name}."):
                    item_path = os.path.join(testing_folder, item)
                    if os.path.isfile(item_path):
                        os.remove(item_path)
                        print(f"Removed file: {item_path}")
                    elif os.path.isdir(item_path):
                        shutil.rmtree(item_path, ignore_errors=False)
                        print(f"Removed folder: {item_path}")
        
        # Remove test-specific results folder if it exists
        if os.path.exists(test_result_folder):
            print(f"Removing folder {test_result_folder}")
            shutil.rmtree(test_result_folder, ignore_errors=False)
            if not os.path.exists(test_result_folder):
                print(f"[INFO] Successfully removed test results folder: {test_result_folder}")
            else:
                print(f"[ERROR] Unable to delete the test results folder '{test_result_folder}'.")
                cleanup_status = False
        
        print(f"[INFO] Cleaned up folders for test: {test_name}")
    else:
        # Clean up all Testing and results folders
        if os.path.exists(testing_folder):
            print("Removing folder " + testing_folder)
            shutil.rmtree(testing_folder, ignore_errors=False)
        if os.path.exists(result_folder):
            print("Removing folder " + result_folder)
            shutil.rmtree(result_folder, ignore_errors=False)

        # Verify if the folder was successfully removed
        if not os.path.exists(testing_folder):
            print(f"[INFO] Successfully removed testing folder : {testing_folder}")
        else:
            print(f"[ERROR] Unable to delete the testing folder '{testing_folder}'.")
            cleanup_status = False

        # Verify if the folder was successfully removed
        if not os.path.exists(result_folder):
            print(f"[INFO] Successfully removed results folder : {result_folder}")
        else:
            print(f"[ERROR] Unable to delete the results folder '{result_folder}'.")
            cleanup_status = False

    if cleanup_status == False:
        return 1
    else:
        return 0


def build(test_dir, test_name=None, unitrace_build_dir=None):
    """Build the test project. If test_name is provided, build only that test."""
    if not os.path.exists(test_dir):
        raise FileNotFoundError(f"[ERROR] Test directory does not exist: {test_dir}")

    # Set up paths
    build_dir = os.path.join(test_dir, "build")
    os.makedirs(build_dir, exist_ok = True)

    # Change to the build directory
    os.chdir(build_dir)
    try:
        print("Build started ... ")
        configure = ['cmake', '..', '-G', 'Ninja']
        if unitrace_build_dir:
            configure.append(f"-DUNITRACE_BUILD_ROOT={unitrace_build_dir}")
        subprocess.run(configure, check = True)
        
        # Build only the specific test if test_name is provided, otherwise build all
        if test_name:
            print(f"Building test: {test_name}")
            subprocess.run(['ninja', test_name], check = True)
        else:
            subprocess.run(['ninja'], check = True)
            
        print("Build completed ...")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] during build: {e}")
        print(f"Return Code: {e.returncode}")
        if e.output:
            print(f"Output: {e.output.decode()}")
        if e.stderr:
            print(f"[ERROR] Output: {e.stderr.decode()}")
        return 1
    except Exception as ex:
        print(f"Unexpected [ERROR]: {ex}")
        return 1

def load_test_definitions():
    """Parse tests.txt into ([defaults] block, list of test blocks)."""
    manifest_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tests.txt")
    blocks = []
    current = None
    with open(manifest_path, 'r') as manifest:
        for line in manifest:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('[') and line.endswith(']'):
                current = {"name": line[1:-1], "command": None, "platform": None,
                           "options": [], "scenarios": [], "skips": [],
                           "defaults_scenario": []}
                blocks.append(current)
                continue
            if current is None:
                raise ValueError(f"line outside a [test] block in {manifest_path}: {line}")
            key, _, value = line.partition('=')
            key, value = key.strip(), value.strip()
            base, _, os_qualifier = key.partition(':')
            os_qualifier = os_qualifier.strip() or None
            if base == "command":
                current["command"] = value
            elif base == "platform":
                current["platform"] = value.split()
            elif base == "options":
                current["options"].append((os_qualifier, value))
            elif base == "scenario":
                current["scenarios"].append((os_qualifier, value))
            elif base == "skip-scenario":
                current["skips"].append((os_qualifier, value))
            elif base == "defaults-scenario":
                current["defaults_scenario"].append((os_qualifier, value))
            else:
                raise ValueError(f"unknown key '{key}' in {manifest_path}")
    defaults = next((b for b in blocks if b["name"] == "defaults"), None)
    if defaults is None:
        raise ValueError(f"no [defaults] block in {manifest_path}")
    return defaults, [b for b in blocks if b["name"] != "defaults"]

def applicable(qualified_values, current_os):
    return [value for (os_qualifier, value) in qualified_values
            if os_qualifier in (None, current_os)]

def resolve_test(test, defaults, current_os):
    """Resolve one test block for one OS: platform gate, scenario list
    (own lines, else defaults), skip-scenario subtraction, options merge.
    Returns the resolved scenario list, or None if the test does not run."""
    platforms = test["platform"] or defaults["platform"] or []
    if current_os not in platforms:
        return None
    scenarios = applicable(test["scenarios"], current_os)
    # defaults-scenario = True/False decides whether the [defaults] list is
    # included; when the key is absent, a block with its own scenario lines
    # replaces the defaults and a block without any inherits them.
    include_defaults = applicable(test["defaults_scenario"], current_os)
    if include_defaults:
        value = include_defaults[-1].strip().lower()
        if value == "true":
            scenarios = applicable(defaults["scenarios"], current_os) + scenarios
        elif value != "false":
            raise ValueError(f"[{test['name']}] defaults-scenario must be "
                             f"True or False, got '{include_defaults[-1]}'")
        elif not scenarios:
            # defaults-scenario = False with no scenario lines: an empty
            # matrix - the test runs once (plus the native run).
            scenarios = [""]
    elif not scenarios:
        scenarios = applicable(defaults["scenarios"], current_os)
    skips = applicable(test["skips"], current_os)
    # A skip matches every scenario containing all of its tokens.
    scenarios = [s for s in scenarios
                 if not any(set(skip.split()) <= set(s.split()) for skip in skips)]
    options = " ".join(applicable(test["options"], current_os))
    if options:
        scenarios = [f"{s} {options}".strip() for s in scenarios]
    return scenarios

def test_name_matches(name, test_name):
    """Exact match, or fnmatch glob when the selector contains wildcards."""
    if any(ch in test_name for ch in "*?["):
        return fnmatch.fnmatch(name, test_name)
    return name == test_name

def build_target_of(test):
    """The ninja target of a test's workload, or None when there is no
    dedicated target (check scripts, python tests) and the whole project
    must be built. A test name is not necessarily a target: e.g.
    selective_kernels builds 'graph' and the event_reset variants build
    'event_reset'."""
    tokens = test["command"].split()
    # skip leading NAME=VALUE environment prefixes
    while tokens and "=" in tokens[0] and not tokens[0].endswith(".py"):
        tokens.pop(0)
    if not tokens or not tokens[0].endswith("run_test.py"):
        return None
    skip_next = False
    for token in tokens[1:]:
        if skip_next:
            skip_next = False
            continue
        if token in ("-e", "-t", "-n", "-c", "-s", "--scenario"):
            skip_next = True
            continue
        if token.startswith("-"):
            continue
        if "{" in token or "/" in token:
            return None
        return token
    return None

def test_group_of(test):
    # python tests are the ones rooted at python_test/
    return "python-test" if "python_test/" in test["command"] else "cpp-test"

def resolve_unitrace_exe(test_dir, unitrace_exe=None):
    if unitrace_exe:
        return unitrace_exe
    exe_name = "unitrace.exe" if platform.system() == "Windows" else "unitrace"
    return os.path.join(test_dir, "..", "..", "build", exe_name)

def build_test_command(test_entry, test_dir, scenario, unitrace_exe=None, skip_golden_compare=False):
    """Expand one tests.txt command line for one scenario.

    Returns the command and the extra environment set by leading NAME=VALUE
    tokens (formerly per-test ctest ENVIRONMENT properties)."""
    tokens = [token.format(python=sys.executable,
                           unitrace=resolve_unitrace_exe(test_dir, unitrace_exe))
              for token in test_entry["cmd"].split()]
    extra_env = {}
    while "=" in tokens[0] and not tokens[0].endswith(".py"):
        name, _, value = tokens.pop(0).partition("=")
        extra_env[name] = value
    if tokens[0].endswith("run_test.py"):
        # --scenario=<value> form: scenario values start with '-'
        tokens.insert(1, f"--scenario={scenario}")
        if unitrace_exe:
            tokens[2:2] = ["--unitrace-exe", unitrace_exe]
        if skip_golden_compare:
            tokens.insert(2, "--skip-golden-compare")
    return [sys.executable] + tokens, extra_env

def launch_test_with_scenarios(test_entry, scenarios, test_dir, total_tests, unitrace_exe=None, skip_golden_compare=False):
    test = test_entry["name"]
    failed_options = []
    sub_tests = 0

    # "native" runs the app without unitrace first, as a driver/environment
    # sanity check; if it fails, skip the unitrace scenarios (not our problem).
    # Commands not using run_test.py ignore the scenario, so they run once.
    if any(token.endswith("run_test.py") for token in test_entry["cmd"].split()):
        run_list = ["native"] + scenarios
    else:
        run_list = ["check"]
    native_failed = False
    for cmd in run_list:
        sub_tests += 1
        command, extra_env = build_test_command(test_entry, test_dir, cmd, unitrace_exe, skip_golden_compare)
        result = subprocess.run(command, capture_output=True, text=True,
                                env={**os.environ, **extra_env},
                                cwd=os.path.dirname(os.path.abspath(__file__)))

        # Keep the per-scenario log layout the ctest harness used, so the
        # failure report and cleanup logic stay unchanged.
        # '.' delimits the test name from the scenario so per-test cleanup
        # cannot match tests whose names extend this one (ze_gemm vs
        # ze_gemm_metric).
        new_dir_name = test+"."+cmd.replace('--', '_').replace('-', '_').replace(" ", "")
        log_dir = os.path.join(test_dir, 'build', 'Testing', f"Temporary_{new_dir_name}")
        os.makedirs(log_dir, exist_ok=True)
        with open(os.path.join(log_dir, "LastTest.log"), 'w') as log_file:
            env_prefix = "".join(f"{k}={v} " for k, v in extra_env.items())
            log_file.write("Command: " + env_prefix + " ".join(command) + "\n")
            log_file.write(result.stdout or "")
            log_file.write(result.stderr or "")

        if result.returncode == 0:
            print(str(total_tests+sub_tests)+" : "+test+cmd+" : Passed")
        else:
            native_failed = cmd == "native"
            failed_options.append((cmd, log_dir))
            error_message = str(total_tests+sub_tests)+" : "+test+cmd+" : Failed"
            RED = '\033[31m'
            RESET = '\033[0m' # Resets text formatting to default
            print(f"{RED}{error_message}{RESET}")

        if native_failed:
            print(f"[ERROR] '{test}' failed to run natively (driver/environment issue, "
                  f"not a unitrace problem); skipping its unitrace scenarios.")
            break
    return sub_tests, failed_options

def run_tests(test_dir, test_group, test_name=None, unitrace_exe=None, skip_golden_compare=False):
    failed_test_flag = 0
    failed_options = []
    total_tests = 0
    current_os = platform.system()

    defaults, tests = load_test_definitions()
    for test in tests:
        if test_group_of(test) != test_group:
            continue
        if test_name is not None and not test_name_matches(test["name"], test_name):
            continue
        scenarios = resolve_test(test, defaults, current_os)
        if scenarios is None:
            continue
        test_entry = {"name": test["name"], "cmd": test["command"]}
        total_sub_tests, tests_status = launch_test_with_scenarios(test_entry, scenarios, test_dir, total_tests, unitrace_exe, skip_golden_compare)
        total_tests += total_sub_tests
        for test_status in tests_status:
            failed_options.append(test_status)
    print("\n\n------------------------------------------------------------")
    print("Total test run "+str(total_tests)+ "  Failed tests : "+str(len(failed_options)))
    print("------------------------------------------------------------")
    if len(failed_options) > 0:
        print("-" * 70)
        print("\n Sub-Test Failure Report:")
        print("{:<20} {:<50} {:<30}".format("[Unitrace Option]", "[Failed Test Log Path]", "[Error Messages]"))
        print("-" * 70)

        failed_test_flag = 1
        for failed_option, path in failed_options:
            error_messages = []

            # Set the correct log file path
            log_file_path = os.path.join(path, "LastTest.log")
            if os.path.exists(log_file_path):
                try:
                    with open(log_file_path, 'r') as log_file:
                        error_block = []
                        capture_error = False

                        for line in log_file:
                            line = line.strip()
                            if '[ERROR]' in line:
                                if capture_error and error_block:
                                    # Save the previous block when a new [ERROR] is found before 'return code'
                                    error_messages.append("\n".join(error_block))
                                    error_block = []
                                capture_error = True

                            # Append lines to the current error block
                            if capture_error:
                                error_block.append(line)

                            # Stop capturing at 'return code' and save the block
                            if ('return code' in line or '<end of output>' in line) and capture_error:
                                error_messages.append("\n".join(error_block))
                                error_block = []
                                capture_error = False

                    # Handle any leftover error block
                    if capture_error and error_block:
                        error_messages.append("\n".join(error_block))

                except Exception as read_error:
                    print(f"[ERROR] reading log file {log_file_path}: {read_error}", file=sys.stderr)
            else:
                print(f"[ERROR] Log file does not exist or is empty at: {log_file_path}")

            # Format the error messages
            if error_messages:
                errors_str = "\n\n".join(error_messages)
            else:
                errors_str = "No [ERROR] found"

            # Print the error information
            print("{:<20} {:<50} {:<30}".format(failed_option, log_file_path, "\n" + errors_str))
    return failed_test_flag

def list_tests(test_group, test_name=None):
    """Print each test's command line and its resolved scenario matrix."""
    current_os = platform.system()
    defaults, tests = load_test_definitions()
    for test in tests:
        if test_group_of(test) != test_group:
            continue
        if test_name is not None and not test_name_matches(test["name"], test_name):
            continue
        scenarios = resolve_test(test, defaults, current_os)
        if scenarios is None:
            print(f"{test['name']}  [not enabled on {current_os}]")
            print(f"  command: {test['command']}")
            print()
            continue
        print(f"{test['name']}")
        print(f"  command: {test['command']}")
        if not any(token.endswith("run_test.py") for token in test["command"].split()):
            print("  scenarios: scenario-independent (single check)")
        elif scenarios == [""]:
            print("  scenarios: none (single run + native)")
        else:
            print(f"  scenarios ({len(scenarios)} + native):")
            for scenario in scenarios:
                print(f"    {scenario}")
        print()
    return 0

def parse_arguments():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="Unitrace Test Script")

    # Optional argument for the test directory
    parser.add_argument(
        "--test-dir",
        type=str,
        default=os.getcwd(),
        help="Path to the test directory. Defaults to the current working directory."
    )
    parser.add_argument(
        "--test-group",
        type=str,
        default="cpp-test",
        help="cpp-test or python-test group to run. default is cpp-test."
    )
    # Flags for actions
    parser.add_argument(
        "--run",
        action="store_true",
        help="Run tests only."
    )
    parser.add_argument(
        "--test-name",
        type=str,
        default=None,
        help="Name of the test to run."
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List each test with its command and resolved scenarios, without running anything."
    )
    parser.add_argument(
        "--unitrace-exe",
        type=str,
        default=None,
        help="Path to the unitrace binary when it is not at the standalone "
             "location <test-dir>/../build/unitrace (e.g. a top-level build "
             "or a prebuilt package)."
    )
    parser.add_argument(
        "--unitrace-build-dir",
        type=str,
        default=None,
        help="Path to the unitrace build directory holding build-time "
             "artifacts the test workloads reuse (ITT library, downloaded "
             "Level Zero headers) when unitrace was built outside the "
             "standalone tools/unitrace layout. Forwarded to the workload "
             "configure step as -DUNITRACE_BUILD_ROOT."
    )
    parser.add_argument(
        "--skip-golden-compare",
        action="store_true",
        help="Skip golden-output content comparisons (for environments "
             "without validated golden files); execution and output checks "
             "still apply. Forwarded to run_test.py."
    )
    return parser.parse_args()

def main():
    args = parse_arguments()
    if args.unitrace_exe:
        args.unitrace_exe = os.path.abspath(args.unitrace_exe)
    if args.unitrace_build_dir:
        args.unitrace_build_dir = os.path.abspath(args.unitrace_build_dir)

    build_target = None
    if args.test_name is not None:
        _, tests = load_test_definitions()
        group_tests = [t for t in tests if test_group_of(t) == args.test_group]
        matched = [t for t in group_tests
                   if test_name_matches(t["name"], args.test_name)]
        if not matched:
            print(f"[ERROR] No test matches '{args.test_name}' in group "
                  f"'{args.test_group}'. Available tests:")
            for t in group_tests:
                print(f"  {t['name']}")
            return 1
        # Build only the matched workload when it is unambiguous; a test
        # name is not necessarily a ninja target (see build_target_of).
        targets = {build_target_of(t) for t in matched}
        if len(targets) == 1:
            build_target = targets.pop()

    if args.list:
        return list_tests(args.test_group, args.test_name)

    if args.test_group == "cpp-test":
        args.test_dir = os.path.join(args.test_dir, "cpp_test")
    elif args.test_group == "python-test":
        args.test_dir = os.path.join(args.test_dir, "python_test")

    if args.run == False:
        status = 0
        # Build the tests now
        if status == 0:
            print("Cleaning up test folder ...")
            status = clean_up_testing_folder(args.test_dir, args.test_name)
            status = build(args.test_dir, build_target, args.unitrace_build_dir)
        if status != 0: #build error check and return
            return status
    print("Start testing...")
    status = run_tests(args.test_dir, args.test_group, args.test_name, args.unitrace_exe, args.skip_golden_compare)
    return(status)

if __name__ == "__main__":
    sys.exit(main())
