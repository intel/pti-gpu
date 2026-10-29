# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes scripts/roofline/roofline_libs.py: Platform (the device config CSV
# from device_configs/) and file_to_json (a unitrace metric query CSV).

import sys
import atheris
import fuzz_common

fuzz_common.add_script_dir("roofline")
import pandas  # noqa: F401  (pre-imported so only unitrace code is instrumented)
with fuzz_common.instrument():
    import roofline_libs

# Platform rejects a malformed config with ValueError (pandas errors for unparsable CSV).
EXPECTED_CONFIG = (pandas.errors.EmptyDataError, pandas.errors.ParserError, ValueError,
                   UnicodeDecodeError)


def TestOneInput(data):
    """Atheris entry point: data is one CSV read both as a device config and as a metric
    query output.

    Loads it with roofline_libs.Platform (the errors in EXPECTED_CONFIG are its clean
    rejection of a malformed config) and converts it with file_to_json. Any other
    exception is a finding.
    """
    with fuzz_common.TempFile(data, ".csv") as path:
        try:
            roofline_libs.Platform(path)
        except EXPECTED_CONFIG:
            pass
        roofline_libs.file_to_json(path)


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
