# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

"""
Utility script for validating PTI and SYCL API IDs.

This script can validate SYCL API IDs against the PTI IDs for SYCL APIs. The
current PTI IDs are Unified Runtime (UR) IDs. We parse the UR headers and
generate interoperable IDs.

TODO: Extend for Intel(R) Level Zero API IDs?
"""

import argparse
import pathlib
import sys
import typing

DEFAULT_PTI_SYCL_API_ID_PUBLIC_HEADER = ["pti/pti_runtime_sycl_api_ids.h"]
DEFAULT_UNIFIED_RUNTIME_PUBLIC_HEADERS = ["ur_api.h", "sycl/ur_api.h"]


def get_public_header_file_path(
    include_dir: pathlib.Path, header_names: typing.List[str]
) -> pathlib.Path:
    """Get the full path to a public header file.

    Args:
        include_dir (pathlib.Path): The base include directory.
        header_names (typing.List[str]): The name of the header file. The first
                                        element in the list takes priority,
                                        the rest are fallbacks. If none of the
                                        files are found, an exception is raised.

    Returns:
        Full path to the header file

    """

    if not include_dir.exists():
        raise FileNotFoundError(f"Include directory {include_dir} does not exist")

    if include_dir.is_file():  # If a file is given, return it directly
        return include_dir

    path_to_header = None
    for header in header_names:
        header_path = include_dir / header
        if header_path.exists() and header_path.is_file():
            path_to_header = header_path
            break

    if not path_to_header:
        raise FileNotFoundError(
            f"None of the specified headers {header_names} were found in {include_dir}"
        )

    return path_to_header


class InternalParsingError(Exception):
    pass


class ApiIdRulesViolationError(Exception):
    pass


class DuplicatedApiIdError(ApiIdRulesViolationError):
    pass


def extract_c_enum_and_value(line: str) -> typing.Tuple[str, int]:
    """Extract enum name and value from a line.

    Args:
        line (str): Line containing enum definition.

    Returns:
        Tuple of enum name and value.
    """
    # Example: ApiName_id = 123, -> ('ApiName_id', '123')
    # i.e., remove everything after comma (avoid comments) and split by '='
    processed_line = tuple("".join(line.strip().split(",")[:-1]).split("="))
    api_name = processed_line[0].strip()
    api_id_value = int(processed_line[1].strip())

    return api_name, api_id_value


class PtiApiIdValidator:
    """
    Class for validating SYCL API IDs against PTI API IDs.
    """

    # PTI Defaults.
    DEFAULT_PTI_SYCL_API_ID_ENUM_START = "typedef enum _pti_api_id_runtime_sycl"

    # SYCL/UR Defaults.
    DEFAULT_UR_API_NAME_COMMENT_PREFIX = "Enumerator for ::"
    DEFAULT_UR_API_ID_ENUM_START = "typedef enum ur_function_t"

    def __init__(
        self,
        pti_include_file: pathlib.Path,
    ):
        self.__pti_ur_api_ids_to_api_map: typing.Dict[int, str] = dict()
        self.__pti_ur_api_to_ids_map: typing.Dict[str, int] = dict()
        self.__load_pti_api_ids(pti_include_file)

    def load_sycl_api_ids(self, ur_include_file: pathlib.Path, raise_on_error: bool):
        found_api_id_enum = False
        current_api_name = None
        with ur_include_file.open("r") as include_file:
            for number, line in enumerate(include_file, 1):
                if not found_api_id_enum and self.DEFAULT_UR_API_ID_ENUM_START in line:
                    found_api_id_enum = True
                elif found_api_id_enum and "FORCE_UINT32" in line:
                    break
                elif found_api_id_enum and line.strip().startswith(
                    "/// " + self.DEFAULT_UR_API_NAME_COMMENT_PREFIX
                ):
                    current_api_name = self.__get_api_name_from_header_line(line)
                elif found_api_id_enum and "=" in line:
                    _, api_id_value = extract_c_enum_and_value(line)
                    if not current_api_name:
                        current_api_name = self.__get_api_name_from_header_line(line)
                        if not current_api_name:
                            raise InternalParsingError(
                                f"{api_id_value} has no associated API name (should never happen!)"
                            )
                    initial_error_msg = f"SYCL/UR API ID value {api_id_value} for {current_api_name} on line {ur_include_file}:{number}"
                    if not f"{current_api_name}_id" in self.__pti_ur_api_to_ids_map:
                        error_msg = (
                            f"{initial_error_msg} not found in PTI SYCL API IDs."
                        )
                        self.__log_or_throw(error_msg, raise_on_error, warning=True)
                    else:
                        pti_existing_api_name = self.__pti_ur_api_ids_to_api_map.get(
                            api_id_value, ""
                        )
                        pti_existing_api_id = self.__pti_ur_api_to_ids_map.get(
                            f"{current_api_name}_id", 0
                        )
                        # If the API ID value exists in PTI, it must match the
                        # 'current' (SYCL/UR) name. However, some API ids can be
                        # versioned. Check if its one of those too.
                        if (
                            pti_existing_api_id != api_id_value
                            and not pti_existing_api_name.startswith(current_api_name)
                        ):
                            error_msg = f"{initial_error_msg} does not match PTI SYCL API ID value {self.__pti_ur_api_to_ids_map[f'{current_api_name}_id']}."
                            self.__log_or_throw(
                                error_msg, raise_on_error, warning=False
                            )
                            if api_id_value in self.__pti_ur_api_ids_to_api_map:
                                error_msg = f"[FATAL] {initial_error_msg} HAS CHANGED in PTI SYCL API IDs."
                                self.__log_or_throw(
                                    error_msg, raise_on_error, warning=False
                                )
                    current_api_name = None

    def __get_api_name_from_header_line(self, line: str) -> str:
        """Extract API name from a line in the header file.

        Args:
            line (str): Line containing API name.

        Returns:
            API name.
        """
        # Example: /// Enumerator for ::urEnqueueKernelLaunch
        # -> 'urEnqueueKernelLaunch'
        return line.split(self.DEFAULT_UR_API_NAME_COMMENT_PREFIX)[1].strip()

    def __log_or_throw(self, message: str, raise_on_error: bool, warning: bool):
        if raise_on_error:
            raise ApiIdRulesViolationError(message)
        else:
            print(f"[{"WARNING" if warning else "ERROR"}] {message}", file=sys.stderr)

    def __load_pti_api_ids(self, pti_include_file: pathlib.Path):
        """Load PTI API Ids into data structures that can be used for validation."""

        found_api_id_enum = False
        with pti_include_file.open("r") as include_file:
            for number, line in enumerate(include_file, 1):
                if (
                    not found_api_id_enum
                    and self.DEFAULT_PTI_SYCL_API_ID_ENUM_START in line
                ):
                    found_api_id_enum = True
                elif found_api_id_enum and "}" in line:
                    break
                elif found_api_id_enum and "=" in line:
                    api_name, api_id_value = extract_c_enum_and_value(line)
                    if api_id_value not in self.__pti_ur_api_ids_to_api_map:
                        self.__pti_ur_api_ids_to_api_map[api_id_value] = api_name
                    else:
                        raise DuplicatedApiIdError(
                            f"Duplicated PTI API ID value found: {api_id_value}. Line {number}"
                        )
                    if api_name not in self.__pti_ur_api_to_ids_map:
                        self.__pti_ur_api_to_ids_map[api_name] = api_id_value
                    else:
                        raise DuplicatedApiIdError(
                            f"Duplicated PTI API name found: {api_name}. Line {number}"
                        )


def main():
    """
    Entry point to the program.

    Run with --help for detailed usage information
    """

    parser = argparse.ArgumentParser(description="Utility for SYCL API IDs")

    parser.add_argument(
        "--pti-include-dir",
        type=pathlib.Path,
        help="Path to PTI include directory",
        required=True,
    )

    parser.add_argument(
        "--ur-include-dir",
        type=pathlib.Path,
        help="Path to Unified Runtime include directory",
        required=False,
        default=None,
    )

    parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate SYCL API IDs against PTI IDs based on spec rules",
    )

    args = parser.parse_args()

    pti_include_file = get_public_header_file_path(
        args.pti_include_dir, DEFAULT_PTI_SYCL_API_ID_PUBLIC_HEADER
    )
    validator = PtiApiIdValidator(pti_include_file)

    if args.ur_include_dir:
        ur_include_file = get_public_header_file_path(
            args.ur_include_dir, DEFAULT_UNIFIED_RUNTIME_PUBLIC_HEADERS
        )
        validator.load_sycl_api_ids(ur_include_file, args.validate)


if __name__ == "__main__":
    main()
