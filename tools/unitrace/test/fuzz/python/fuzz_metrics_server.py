# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes the two non-CSV inputs of scripts/metrics/analyzeperfmetrics.py:
#   - a request to the localhost metrics server (PerfMetricsHTTPServer do_GET),
#     ParseMetricsRequest(); a returned instance must convert with float() and a
#     device with int(), as do_GET converts them
#   - the -f/--config options file, ReadOptionsFromFile via ParseArguments()
# Input: one flag byte (bit 0: config file, bit 1: --result-dir, bit 2: stall
# sampling), then the request path or the file contents.

import sys
import atheris
import fuzz_common

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot  # noqa: F401  (pre-imported so only unitrace code is instrumented)
import pandas  # noqa: F401
from matplotlib.backends.backend_pdf import PdfPages  # noqa: F401

fuzz_common.add_script_dir("metrics")
with fuzz_common.instrument():
    import analyzeperfmetrics as apm


def TestOneInput(data):
    """Atheris entry point: data is one flag byte, then a request path or an options file.

    Flag bit 0 set: the rest is a -f/--config options file, parsed by ParseArguments
    (argparse exiting or reporting the file unreadable is a clean rejection).
    Otherwise the rest is a metrics server request path (decoded as latin-1, as
    http.server does), parsed by ParseMetricsRequest with bit 1 as --result-dir and
    bit 2 as stall sampling; an accepted request's instance must convert with
    float() and its device with int(), as do_GET converts them. Any other
    exception is a finding.
    """
    if not data:
        return
    flags, body = data[0], data[1:]
    if flags & 1:
        with fuzz_common.TempFile(body, ".txt") as path:
            try:
                apm.ParseArguments(["-f", path, "input.csv"])
            except SystemExit:
                pass  # argparse rejected the options and printed why
            except UnicodeDecodeError:
                pass  # argparse reports the file as unreadable
        return

    # The server gets the path as http.server decodes the request line: latin-1.
    path = body.decode("latin-1")
    status, kname, instance, device_id = apm.ParseMetricsRequest(path, bool(flags & 2), bool(flags & 4))
    if status == "ok":
        float(instance)
        if device_id is not None:
            int(device_id)


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
