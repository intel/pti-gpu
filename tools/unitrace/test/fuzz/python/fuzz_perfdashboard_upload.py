# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes the CSV upload of the counter dashboard, scripts/metrics/perfdashboard.py
# upload_csv_input1(): the browser sends the file base64-encoded and the Bokeh
# callback reads it (read_metrics), maps its counter columns and indexes its kernel
# instances. An exception escaping the callback leaves the dashboard half-updated.
# The Bokeh models are built in memory; no server runs.

import base64
import sys
import atheris
import fuzz_common

import matplotlib
matplotlib.use("Agg")
import pandas  # noqa: F401
import bokeh.plotting  # noqa: F401  (pre-imported so only unitrace code is instrumented)
import bokeh.models  # noqa: F401
import bokeh.layouts  # noqa: F401

fuzz_common.add_script_dir("metrics")
with fuzz_common.instrument():
    import perfdashboard as pd_app


def TestOneInput(data):
    """Atheris entry point: data is one CSV file uploaded to the counter dashboard.

    Calls the Bokeh upload callback with the file base64-encoded, as the browser
    sends it. Any exception escaping the callback is a finding.
    """
    pd_app.upload_csv_input1("value", None, base64.b64encode(data).decode("ascii"))


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
