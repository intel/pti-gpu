# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes scripts/metrics/analyzeperfmetrics.py on an untrusted metrics CSV, along
# the two paths that read one:
#   - perfdashboard.py read_metrics(): ScanDeviceSections + ReadMetricFrame, unguarded
#   - analyzeperfmetrics.py main(): MetricsInfo, -l listing (print_data), and for a
#     stall table the stall report (StallColumns, NonZeroStallRows,
#     KernelRanges, BuildStallReport)
# Plotting to PDF and the HTTP server are out of scope.

import argparse
import csv
import io
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

# csv.Error: a field above csv.field_size_limit(); analyzeperfmetrics.main and
# perfdashboard.upload_csv_input1 both catch it.
EXPECTED = (UnicodeDecodeError, csv.Error)
REPORT_ARGS = argparse.Namespace(numtopstalls=3)


def TestOneInput(data):
    """Atheris entry point: data is one metrics CSV.

    Reads it as perfdashboard.py does (ScanDeviceSections, ReadMetricFrame), then as
    analyzeperfmetrics.py main() does (MetricsInfo, the -l listing and, for stall
    tables, the stall report). Only the errors in EXPECTED, and those main() itself
    turns into an error message, are a clean rejection; any other exception is a
    finding.
    """
    with fuzz_common.TempFile(data, ".csv") as path:
        try:
            apm.ScanFirstHeader(path)
            for device, header, last, kind in apm.ScanDeviceSections(path):
                apm.ReadMetricFrame(path, header, last, kind)
        except EXPECTED:
            return

        try:
            info = apm.MetricsInfo(input_path=path, is_result_directory=False)
        except Exception:
            return  # main() reports these as "Error: ..." and exits
        info.print_data(path + ".out")
        for device in info.devices.values():
            df = device.get_device_data()
            if df is None or df.shape[0] == 0 or device.kind != "stall":
                continue
            stalls = apm.StallColumns(df)
            if len(stalls) == 0:
                continue
            for kernel, pid, kernelid, start, stop in apm.KernelRanges(df):
                df2 = apm.NonZeroStallRows(df.iloc[start:stop], stalls)
                if df2.shape[0] == 0:
                    continue
                apm.BuildStallReport(apm.StallKernelLabel(kernel, pid, kernelid), REPORT_ARGS,
                                     df2, stalls)


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
