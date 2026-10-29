# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes scripts/tracemerge/mergetrace.py: merge_trace_files (Chrome JSON,
# including its repair-and-retry of a truncated file) and MergePerfettoTraces.
# Oracle: a successful JSON merge must itself be valid JSON.

import json
import sys
import atheris
import fuzz_common

fuzz_common.add_script_dir("tracemerge")
with fuzz_common.instrument():
    import mergetrace


def TestOneInput(data):
    """Atheris entry point: data is one trace file.

    Merges it as a Chrome JSON trace (merge_trace_files, with its repair of a
    truncated file) and checks that a merge it reports as successful is valid JSON
    (not NaN or Infinity, which json.load would accept, nor a number that overflows
    a float), then merges it as Perfetto traces (MergePerfettoTraces). Any exception
    is a finding.
    """
    with fuzz_common.TempFile(data, ".json") as path:
        out = path + ".out"
        if mergetrace.merge_trace_files([path], out):
            with open(out) as f:
                json.load(f, parse_constant=mergetrace.RejectJsonConstant,
                          parse_float=mergetrace.ParseFiniteFloat)
        mergetrace.MergePerfettoTraces([path], out)


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()
