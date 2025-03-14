# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

"""
Script for generating coverage reports with LLVM's source-based code coverage.

https://clang.llvm.org/docs/SourceBasedCodeCoverage.html

The motivation for this script is providing a wrapper for llvm-cov. This
utility is cumbersome to use with multiple binaries, e.g., if your project has
 many test binaries, one must pass `--object <BIN>` (without globs)
into llvm-cov.

Before using this script, make sure you run the `llvm-profdata` merge tool and
get a profdata file. Also, I would generally recommend aggregating binaries
into a single directory.
"""


import argparse
import pathlib
import subprocess

DEFAULT_LLVM_COV = "llvm-cov"
# These are most likely not binaries w/ coverage data
DEFAULT_OBJECT_EXTENSION_EXCLUSION = [
    ".cc",
    ".cpp",
    ".cmake",
    ".txt",
    ".cxx",
    ".h",
    ".in",
    ".hpp",
    ".spv",
    ".a",
    ".cl",
    ".pdb",
    ".lib",
    ".profraw",
    ".profdata",
]
DEFAULT_OUTPUT_DIR = pathlib.Path("./coverage")
DEFAULT_COVERAGE_DATA_FILE = pathlib.Path("./cov.profdata")
DEFAULT_LCOV_EXPORT_FILE = pathlib.Path("./cov.info")


def gen_object_list(obj_arg, extension_filter):
    """Generate list of objects to pass to llvm-cov

    obj_arg - user arg with either file (object) or directory with objects
    extension_filter - extensions to ignore.
    """

    obj_list = []

    for obj in obj_arg:
        if not obj.exists():
            continue
        if obj.is_file() and obj.suffix not in extension_filter:
            obj_list.append(obj)
        if obj.is_dir():
            # Recursive support?
            obj_list.extend(
                [
                    dir_file
                    for dir_file in obj.iterdir()
                    if dir_file.exists()
                    and dir_file.is_file()
                    and dir_file.suffix not in extension_filter
                ]
            )

    return obj_list


def base_llvm_cmd(cov_cmd, subcommand, merge_data, objects, sources, extra):
    """Return common llvm-cov command as list"""

    cmd = []
    cmd.append(f"{cov_cmd!s}")
    cmd.append(subcommand)
    cmd.append(f"--instr-profile={merge_data!s}")
    cmd.extend([f"--object={my_obj!s}" for my_obj in objects])

    if sources is not None:
        cmd.append("--sources")
        cmd.extend([f"{sources_dir!s}" for sources_dir in sources])

    if extra is not None:
        cmd.extend(extra.split(";"))

    return cmd


class LlvmCovHelper:
    """
    Helper class to generate coverage reports using llvm-cov.

    This class provides methods to generate HTML coverage reports,
    summarize coverage, and export coverage data in lcov format.
    """

    def __init__(
        self, cov_cmd, merge_data, objects, sources=None, extra=None, verbose=True
    ):
        self.verbose = verbose
        self.show_cmd = base_llvm_cmd(
            cov_cmd, "show", merge_data, objects, sources, extra
        )
        self.export_cmd = base_llvm_cmd(
            cov_cmd, "export", merge_data, objects, sources, extra
        )
        self.report_cmd = base_llvm_cmd(
            cov_cmd, "report", merge_data, objects, sources, extra
        )

    def html(self, outputdir):
        """Generate html with source-based coverage"""

        html_cmd = self.show_cmd + ["--format=html"] + [f"--output-dir={outputdir!s}"]

        if self.verbose:
            print(*html_cmd)

        subprocess.run(html_cmd, check=True)

    def summary(self):
        """Generate a console summary with source-based coverage"""

        if self.verbose:
            print(*self.report_cmd)

        subprocess.run(self.report_cmd, check=True)

    def export_lcov(self, file_name):
        """Export source-based coverage data into lcov format."""

        lcov_cmd = self.export_cmd + ["--format=lcov"]

        if self.verbose:
            print(*lcov_cmd)

        exported_lcov = subprocess.run(
            lcov_cmd,
            stdout=subprocess.PIPE,
            universal_newlines=True,
            check=True,
        )

        with file_name.open("w") as lcov_info:
            lcov_info.write(exported_lcov.stdout)


def main():
    """
    Entry point to the program.

    Run with --help for detailed usage information
    """

    parser = argparse.ArgumentParser(
        description="Utility generating coverage reports with llvm-cov."
    )

    parser.add_argument(
        "--llvm-cov",
        type=pathlib.Path,
        default=DEFAULT_LLVM_COV,
        help=f"Default llvm-cov command. Defaults to {DEFAULT_LLVM_COV}",
    )

    parser.add_argument(
        "--objects",
        nargs="+",
        required=True,
        type=pathlib.Path,
        help=(
            "Objects to generate report with (pass into llvm-cov..)."
            " Can either be a directory or binary file."
        ),
    )

    parser.add_argument(
        "--data",
        type=pathlib.Path,
        default=DEFAULT_COVERAGE_DATA_FILE,
        help=(
            "Merged coverage data (profdata format)."
            f" Defaults to {DEFAULT_COVERAGE_DATA_FILE}"
        ),
    )
    parser.add_argument(
        "--exclude",
        nargs="+",
        default=DEFAULT_OBJECT_EXTENSION_EXCLUSION,
        help="List of extensions to exclude from objects.",
    )
    parser.add_argument(
        "--lcov",
        type=pathlib.Path,
        required=False,
        help=(
            "File in lcov format to export coverage data into"
            " (e.g. coverage.info file one can pass into `genhtml`)."
        ),
    )

    parser.add_argument(
        "--summary",
        action="store_true",
        help="Generate coverage summary Report.",
    )

    parser.add_argument(
        "--sources",
        nargs="+",
        type=pathlib.Path,
        help="Source code to generate coverage data.",
    )

    parser.add_argument(
        "--output-dir",
        type=pathlib.Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Default output directory for html reports (default: coverage).",
    )

    parser.add_argument(
        "--extra",
        type=str,
        help=(
            "Semi-colon separated list of extra args to pass to llvm-cov."
            " Must use '=' instead of ' ', e.g., --extra=\"-args\"."
        ),
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Verbose output.",
    )

    args = parser.parse_args()

    obj_list = gen_object_list(args.objects, args.exclude)

    coverage_generation = LlvmCovHelper(
        args.llvm_cov, args.data, obj_list, args.sources, args.extra, args.verbose
    )

    if args.summary:
        coverage_generation.summary()
        return

    if args.lcov:
        coverage_generation.export_lcov(args.lcov)
        return

    coverage_generation.html(args.output_dir)


if __name__ == "__main__":
    main()
