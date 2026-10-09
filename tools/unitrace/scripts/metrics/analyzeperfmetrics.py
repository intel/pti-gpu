#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import io
import csv
import socket
import ssl
import sys
import re
import shlex
import subprocess
import argparse
import tempfile
import pandas as pd
import matplotlib.pyplot as plt

from itertools import islice

from matplotlib.backends.backend_pdf import PdfPages as pdf
from http.server import HTTPServer, ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import unquote
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Union

@dataclass
class DeviceInfo:
    """One device's metric table: which file holds it and which lines of that file."""
    device_id: int
    metrics_file: Path
    header_line: int
    kind: str                                           # what ClassifyHeaderRow made of the header row
    last_line: Optional[int] = field(default = None)    # None means "to the end of the file"

    def __post_init__(self):
        if not (self.metrics_file.exists() and
                self.metrics_file.is_file() and
                self.metrics_file.stat().st_size > 0):
            raise ValueError(
                f"DeviceInfo for device {self.device_id}: "
                f"File '{self.metrics_file}' does not exist, is not a file, or is empty"
        )

    def get_device_data(self) -> Optional[pd.DataFrame]:
        """Read the device's metric table into a DataFrame."""
        try:
            return ReadMetricFrame(self.metrics_file, self.header_line, self.last_line, self.kind)
        except Exception as e:
            print(f"Error reading device file {self.metrics_file}: {e}")
            return None

    def _print_data_from_df(self, df, of):
        """Print the device's metrics and kernels to of."""
        if (self.kind != "stall"):
            for m in df.columns.values.tolist():
                if (m == 'Kernel'):
                    continue
                print("        " + m, file = of)
                if (m == 'StreamMarker'):
                    break

            kernel_and_instances = dict()
            kernel = ""
            for index, row in df.iterrows():
                if (pd.isna(row['Kernel']) == True):
                    if kernel in kernel_and_instances:
                        kernel_and_instances[kernel] = kernel_and_instances[kernel] + 1
                    else:
                        if (kernel != ""):
                            kernel_and_instances[kernel] = 1

                    kernel = ""	# reset kernel name to empty
                else:
                    if (kernel == ""):
                        kernel = row['Kernel']

            print("    Kernel, Number of Instances", file = of)
            for kernel, instances in kernel_and_instances.items():
                print("        \"" + kernel + "\", " + str(instances), file = of)
        else:
            for m in df.columns.values.tolist():
                if (m in STALL_INFO_COLUMNS):   # describes the instruction, not a metric
                    continue
                print("        " + m, file = of)

            # By identity, so same-named kernels are separate entries; -k takes the
            # quoted name.
            kernels = []
            for kernel, pid, kernelid, start, stop in KernelRanges(df):
                if ((kernel, pid, kernelid) not in kernels):
                    kernels.append((kernel, pid, kernelid))

            print("    Kernel, Pid, KernelId", file = of)
            for kernel, pid, kernelid in kernels:
                print("        \"" + str(kernel) + "\", " + str(pid) + ", " + str(kernelid), file = of)

    def print_data(self, of):
        df = self.get_device_data()
        print("Device " + str(self.device_id), file=of)
        if df is None:
            print(f"    No data available for device {self.device_id}", file=of)
            return

        print("    Metric", file = of)
        self._print_data_from_df(df, of)

@dataclass
class MetricsInfo:
    """Database containing global info and all devices information."""
    input_path: Union[Path, str]
    is_result_directory: bool
    is_eustall: bool = field(init=False, default=False)
    devices: Dict[int, DeviceInfo] = field(init=False, default_factory=dict)

    def __post_init__(self):
        self._validate_input()
        self._init_devices()
        self._init_is_eustall()

    def _init_is_eustall(self):
        """Whether the lowest-numbered device's table is a stall table."""
        if not self.devices:
            return

        self.is_eustall = (self.devices[min(self.devices)].kind == "stall")

    def _init_devices(self):
        if self.is_result_directory:
            self._init_devices_from_directory()
        else:
            self._init_devices_from_single_file()

    def _init_devices_from_directory(self):
        """For result_directory mode, find metrics/metrics_<device>.csv files."""
        filename_pattern = re.compile(r'^metrics_(\d+)\.csv$')

        for metrics_file in sorted(self.input_path.rglob('metrics/metrics_*.csv')):
            match = filename_pattern.match(metrics_file.name)
            if match and metrics_file.stat().st_size > 0:
                try:
                    device_id = int(match.group(1))
                except (ValueError, OverflowError) as e:
                    print(f"Warning: Invalid device ID in {metrics_file.name}: {e}")
                    continue
                # One device per file, named by the file; its table runs to the end.
                header_line, kind = ScanFirstHeader(metrics_file)
                if kind is None:
                    print(f"Warning: No metrics table found in {metrics_file}")
                    continue
                self.devices[device_id] = DeviceInfo(device_id=device_id,
                                                     metrics_file=metrics_file,
                                                     header_line=header_line,
                                                     kind=kind)
        if not self.devices:
            print(f"Warning: No valid metrics files found in directory {self.input_path}")

    def _init_devices_from_single_file(self):
        """Find the devices in a single metrics file."""
        for device_id, header_line, last_line, kind in ScanDeviceSections(self.input_path):
            self.devices[device_id] = DeviceInfo(device_id=device_id,
                                                 metrics_file=self.input_path,
                                                 header_line=header_line,
                                                 last_line=last_line,
                                                 kind=kind)

    def _validate_input(self):
        if isinstance(self.input_path, str):
            self.input_path = Path(self.input_path).resolve()
        elif isinstance(self.input_path, Path):
            self.input_path = self.input_path.resolve()
        else:
            raise TypeError(f"input_path must be a Path object or string, got {type(self.input_path).__name__}")

        if not isinstance(self.is_result_directory, bool):
            raise TypeError(f"is_result_directory must be a boolean, got {type(self.is_result_directory).__name__}")

        if not self.input_path.exists():
            raise FileNotFoundError(f"Input path '{self.input_path}' does not exist.")

        if self.is_result_directory:
            if not self.input_path.is_dir():
                raise NotADirectoryError(f"'{self.input_path}' is not a directory")
        else:
            if not self.input_path.is_file():
                raise FileNotFoundError(f"'{self.input_path}' is not a file")
            if self.input_path.stat().st_size == 0:
                raise ValueError(f"File '{self.input_path}' is empty")

    def get_device(self, device_id: int) -> Optional[DeviceInfo]:
        """Get device info by ID."""
        return self.devices.get(device_id)

    def get_device_ids(self) -> List[int]:
        """Get sorted list of all device IDs."""
        return sorted(self.devices.keys())

    def print_data(self, output=None):
        of = sys.stdout
        if (output is not None):
            of = open(output, "w")
        for device_id in self.get_device_ids():
            self.devices[device_id].print_data(of)
        if (output is not None):
            of.close()

class ReadOptionsFromFile(argparse.Action):
    def __call__ (self, parser, namespace, values, option_string = None):
        inputfile = vars(namespace)['input']    # save the input file argument
        with values as f:
            options = shlex.split(f.read().strip())
            options.append("dummyinput")    # parse_args() requires an input file
            parser.parse_args(options, namespace)
            vars(namespace)['input'] = inputfile # restore input file

def ParseArguments(argv):
    argparser = argparse.ArgumentParser(description = "GPU Kernel Performance Hardware Metrics Analyzer")
    argparser.add_argument('-l', '--list', action = 'store_true', help = "list devices, metrics and kernels in the input performance metric data file")
    argparser.add_argument('-d', '--device', type = int, default = 0, help = "GPU device (device 0 by default)")
    argparser.add_argument('-k', '--kernel', help = "kernel name, with shape for query or time-based data, all kernels if this option is not specified")
    argparser.add_argument('-i', '--instance', type = int, default = 1, help = "kernel instance. all instances if < 1 (1 or the first instance by default)")
    argparser.add_argument('-m', '--metrics', action = 'append', help = "list of comma-separated metric names")
    argparser.add_argument('-b', '--throughput', action = 'append', help = "list of comma-separated metric names for throughput")
    argparser.add_argument('-o', '--output', help = "output file in text format if -l is present, PDF format regardless of file extension if -k is not present or if instance < 1")
    argparser.add_argument('-p', '--https', action = 'store_true', help = "start https server for backward compatibility only to view metrics with https link")
    argparser.add_argument('-q', '--http', action = 'store_true', help = "start http server")
    argparser.add_argument('-c', '--certificate', help = "certificate file for https server")
    argparser.add_argument('-e', '--key', help = "private key file for https server")
    argparser.add_argument('-n', '--numtopstalls', type = int, default = 10, help = "number of top most expensive stalls of each type to report for stall analysis(10 default, -1 unlimited)")
    argparser.add_argument('-r', '--report', help = "stall analysis report in plain text")
    argparser.add_argument('-y', '--ylabel', action = 'append', help = "label for Y axis")
    argparser.add_argument('-x', '--xlabel', default = "Time(in sampling intervals)", help = "label for X axis (defaut is \"Time(in sampling intervals)\")")
    argparser.add_argument('-t', '--title', default = "Performance Metrics", help = "performance metric plot title")
    argparser.add_argument('-f', '--config', type = open, action = ReadOptionsFromFile, help = "read command options from file")
    argparser.add_argument('--result-dir', action = 'store_true', help = "Process metrics from structured result directory instead of single CSV file")
    argparser.add_argument('input', help = 'hardware performance metric data file in .csv format generated by unitrace -k/--stall-sampling')

    return argparser.parse_args(argv)

# Every collector writes sections, one per kernel (stall sampling) or kernel instance
# (query, time-based sampling), each opened by a label line naming its identity columns:
#     Kernel,Pid,KernelId           stall sampling
#     Kernel, GlobalInstanceId      query and time-based sampling
# (a query section has a row per sub-device, and a first column SubDeviceId only if there
# is more than one). The stall label is matched in full, first; a first column of
# "Kernel" routes the other two to one reader.
# Labels are compared field by field, ignoring spacing (NormalizeFields).

# A stall section's identity, copied onto its rows. Pid and KernelId also name the
# kernel's <device>.<pid>.<kernel id>.asm file.
STALL_IDENTITY_COLUMNS = ["Kernel", "Pid", "KernelId"]

# The first column of every label line, and of no other line.
BLOCK_LABEL_FIRST_COLUMN = "Kernel"

# Stall columns that are not metrics; every column after Dependency is one.
STALL_INFO_COLUMNS = STALL_IDENTITY_COLUMNS + ["Source", "Offset", "Instruction", "Dependency"]

# What unitrace writes for an unresolved source or dependency list, and for an
# instruction it could not disassemble.
NOT_AVAILABLE = "NA"
UNKNOWN_INSTRUCTION = "unknown"

def NormalizeFields(fields):
    """Fields stripped of surrounding whitespace; applied once per row, before any comparison."""
    return [f.strip() for f in fields]

def IsBlockLabel(fields):
    """Whether the normalized fields open a section: a first column of Kernel and at least one more.

    Only the first column is checked; the rest name the identity columns, whatever they are.
    """
    return ((len(fields) >= 2) and (fields[0] == BLOCK_LABEL_FIRST_COLUMN))

def ClassifyHeaderRow(row):
    """The kind of table a label line opens: "stall", "time" or None."""
    fields = NormalizeFields(next(csv.reader([row], skipinitialspace = True), []))
    if (fields == STALL_IDENTITY_COLUMNS):
        return "stall"
    if (IsBlockLabel(fields)):
        return "time"

    return None

def ScanFirstHeader(inputfile):
    """(line, kind) of the first label line of a one-device file, or (None, None)."""
    with open(inputfile, "r") as f:
        for linenum, row in enumerate(f):
            kind = ClassifyHeaderRow(row)
            if (kind is not None):
                return linenum, kind

    return None, None

def ScanDeviceSections(inputfile):
    """Locate every device's metric table in a metrics file.

    Returns a list of (device, header, last, kind): header is the 0-based line of the
    device's first label line, last the line after its table (None for the end of the
    file), and kind ClassifyHeaderRow's. A device with no label line is left out.
    """
    sections = []
    device = 0		# a file with no banner at all is one unnamed device
    header = None
    kind = None
    linenum = 0

    with open(inputfile, "r") as f:
        for row in f:
            if (("=== Device" in row) and ("Metrics ===" in row)):
                if (header is not None):	# the banner ends the section before it
                    sections.append((device, header, linenum, kind))
                    header = None
                    kind = None
                for word in row.split():	# "=== Device #N Metrics ==="
                    if (word.startswith('#')):
                        try:
                            device = int(word[1:])
                        except ValueError:
                            print(f"Warning: Could not parse device ID from '{word}'")
                        break
            elif (header is None):
                kind = ClassifyHeaderRow(row)
                if (kind is not None):
                    header = linenum
            linenum += 1

    if (header is not None):
        sections.append((device, header, None, kind))

    return sections

def ReadStallFrame(inputfile, header, last):
    """Read one device's stall sections, lines header to last, into one frame.

    Each section's Kernel, Pid and KernelId are copied onto its rows. The csv module
    handles the quoted cells, which contain commas; skipinitialspace lets a ", " delimiter
    through too.
    """
    identity = None
    columns = None
    rows = []
    pending = None

    with open(inputfile, "r", newline = "") as f:
        # Sliced lazily by line, since the table is bounded by line numbers.
        for fields in csv.reader(islice(f, header, last), skipinitialspace = True):
            if (len(fields) == 0):
                # The blank line between sections, which must not fill a pending slot.
                continue
            fields = NormalizeFields(fields)
            if (fields == STALL_IDENTITY_COLUMNS):
                pending = "identity"
                continue
            if (pending == "identity"):
                identity = fields[0 : len(STALL_IDENTITY_COLUMNS)]
                pending = "columns"
                continue
            if (pending == "columns"):
                if (columns is None):
                    columns = fields		# identical across every block
                pending = None
                continue
            # A row of the current section; wrong-width lines are skipped.
            if ((columns is not None) and (identity is not None)
                    and (len(identity) == len(STALL_IDENTITY_COLUMNS)) and (len(fields) == len(columns))):
                rows.append(identity + fields)

    if (columns is None):
        return pd.DataFrame()

    df = pd.DataFrame(rows, columns = STALL_IDENTITY_COLUMNS + columns)

    # The counts to integers; the text columns stay strings, "NA" included.
    numeric = ["Pid", "KernelId"]
    if ("Dependency" in columns):
        numeric += columns[columns.index("Dependency") + 1:]
    for column in numeric:
        df[column] = pd.to_numeric(df[column], errors = "coerce").fillna(0).astype("int64")

    return df

def ReadTimedFrame(inputfile, header, last):
    """Read one device's query or time-based sections, lines header to last, into one frame.

    Each section (label, identity, counter names, one row per sample, or one for query) has
    its identity, whose columns the label names, copied onto its rows. Instances are ended
    by an all-NaN row. skipinitialspace matches the writer's ", " delimiter.
    """
    identity_columns = None
    identity = None
    columns = None
    rows = []
    pending = None

    with open(inputfile, "r", newline = "") as f:
        # Sliced lazily by line, since the table is bounded by line numbers.
        for fields in csv.reader(islice(f, header, last), skipinitialspace = True):
            if (len(fields) == 0):
                # The blank line between sections, which must not fill a pending slot.
                continue
            fields = NormalizeFields(fields)
            if (IsBlockLabel(fields)):
                # A new section: end the previous instance with an all-NaN row.
                if (len(rows) > 0):
                    rows.append([float("nan")] * len(rows[-1]))
                if (identity_columns is None):
                    identity_columns = fields		# identical across every block
                identity = None
                pending = "identity"
                continue
            if (pending == "identity"):
                identity = fields[0 : len(identity_columns)]
                pending = "columns"
                continue
            if (pending == "columns"):
                if (columns is None):
                    columns = fields		# identical across every block
                pending = None
                continue
            # A row of the current section; wrong-width lines are skipped.
            if ((columns is not None) and (identity is not None)
                    and (len(identity) == len(identity_columns)) and (len(fields) == len(columns))):
                rows.append(identity + fields)

    if (columns is None):
        return pd.DataFrame()

    # End the last instance too.
    if (len(rows) > 0):
        rows.append([float("nan")] * (len(identity_columns) + len(columns)))

    df = pd.DataFrame(rows, columns = identity_columns + columns)

    # Everything but Kernel to numbers.
    for column in df.columns:
        if (column != "Kernel"):
            df[column] = pd.to_numeric(df[column], errors = "coerce")

    return df

def ReadMetricFrame(inputfile, header, last, kind):
    """Read one device's metric table, lines header to last."""
    if (kind == "stall"):
        return ReadStallFrame(inputfile, header, last)

    return ReadTimedFrame(inputfile, header, last)

def StallColumns(df):
    """The stall count columns: those after Dependency, except Active[Events]."""
    cols = list(df.columns)
    if ("Dependency" not in cols):
        return []

    return [c for c in cols[cols.index("Dependency") + 1:] if c != "Active[Events]"]

def NonZeroStallRows(df, stalls):
    """Drop the rows in which no stall of any type was sampled."""
    if (len(stalls) == 0):
        return df.iloc[0:0]

    return df[(df[stalls] != 0).any(axis = 1)]

def KernelRanges(df):
    """Split the frame into (kernel, pid, kernelid, start, stop) ranges, one per kernel.

    A change of any of the three identity columns ends a range, so same-named kernels
    stay apart. start/stop are positions for iloc.
    """
    # an empty frame has no columns
    if (not set(STALL_IDENTITY_COLUMNS).issubset(df.columns)):
        return []

    ranges = []
    identity = None
    start = 0
    for index, row in enumerate(zip(df['Kernel'], df['Pid'], df['KernelId'])):
        if (identity is None):
            identity = row
            start = index
        else:
            if (identity != row):
                ranges.append(identity + (start, index))
                identity = row
                start = index

    # don't forget the last kernel
    if (identity is not None):
        ranges.append(identity + (start, df.shape[0]))

    return ranges

def StallKernelLabel(kernel, pid, kernelid):
    """How a stall report names a kernel; pid and kernel id also name its .asm file."""
    return str(kernel) + " (pid " + str(pid) + ", kernel id " + str(kernelid) + ")"

DEPENDENCY_SEPARATOR = " | "
# One producer, as FormatDependencies() in src/levelzero/ze_metrics.h writes it:
# "0x<offset> (<source>)[: <text>]", joined by DEPENDENCY_SEPARATOR.
DEPENDENCY_ENTRY = re.compile(r"^(0x[0-9a-fA-F]+)(?:\s+\(([^)]*)\))?(?::\s(.*))?$")

def ParseDependency(cell):
    """Split a Dependency cell into (offset, source, text) producers.

    Missing parts are None; an unrecognized entry is kept as (None, None, entry).
    """
    producers = []
    if ((cell is None) or (cell == "") or (cell == NOT_AVAILABLE)):
        return producers

    for entry in cell.split(DEPENDENCY_SEPARATOR):
        entry = entry.strip()
        if (entry == ""):
            continue
        m = DEPENDENCY_ENTRY.match(entry)
        if (m is None):
            producers.append((None, None, entry))
        else:
            producers.append((m.group(1), m.group(2), m.group(3)))

    return producers

def FormatStallInstruction(offset, source, text, indent, missing = "<no disassembly available>"):
    """One instruction of a stall report: offset, text and source."""
    if ((text is None) or (text == "") or (text == UNKNOWN_INSTRUCTION)):
        text = missing

    line = indent
    if ((offset is not None) and (offset != "")):
        line += str(offset) + ": "
    line += text + "\n"

    if ((source is not None) and (source != "") and (source != NOT_AVAILABLE)):
        line += indent + "Source: " + source + "\n"

    return line

def BuildStallReport(label, args, stalldf, stalls):
    """Report a kernel's most expensive stalls and the instructions they wait on.

    The instructions, sources and producers all come from the metrics file. label is
    StallKernelLabel's.
    """
    report = "Kernel: " + label + "\n"
    if (args.numtopstalls >= 0):
        report += "\nReport of Top " + str(args.numtopstalls) + " Stalls of Each Type\n"
    else:
        report += "\nReport of Stalls of Each Type\n"

    totals = []
    for t in stalls:
        total = int(stalldf[t].sum())
        if (total > 0):
            totals.append((t, total))

    report += "***********************************************************************************************\n"
    report += "Stall events by type: \n"
    if (len(totals) == 0):
        report += "  none\n"
    else:
        for t, total in sorted(totals, key = lambda e: e[1], reverse = True):
            report += "  " + t + ": " + str(total) + "\n"

    if ((stalldf["Instruction"] == UNKNOWN_INSTRUCTION).any()):
        report += "\nSome instructions could not be disassembled and are reported by offset only.\n"

    # one section per stall type, in the order the metrics file lists them
    for t in stalls:
        df = stalldf[stalldf[t] > 0]    # drop 0s
        if (df.shape[0] == 0):  # zero stalls of this type. move to the next one
            continue

        # Offset breaks ties, for a stable order.
        df = df.sort_values(by = [t, "Offset"], ascending = [False, True])

        report += "***********************************************************************************************\n"
        report += t.split("Stall")[0] + " Stalls: \n"

        num_stalls_reported = 0
        for index, row in df.iterrows():
            if ((args.numtopstalls >= 0) and (num_stalls_reported >= args.numtopstalls)):
                break

            report += "\nInstruction\n"
            report += FormatStallInstruction(row["Offset"], row["Source"], row["Instruction"], "  ")
            report += "  " + t + ": " + str(row[t])
            if ("Active[Events]" in row.index):
                report += "   (Active[Events]: " + str(row["Active[Events]"]) + ")"
            report += "\n"

            producers = ParseDependency(row["Dependency"])
            if (len(producers) == 0):
                report += "is stalled (no dependency information)\n"
            else:
                report += "is stalled potentially by\n"
                for offset, source, text in producers:
                    report += "  instruction\n"
                    report += FormatStallInstruction(offset, source, text, "    ", missing = "<instruction not available>")

            num_stalls_reported = num_stalls_reported + 1

    report += "===============================================================================================\n"
    return report

def WriteOutStallReport(report, p): # p is PDF object
    if (p is None): # p should already be created, but just in case
        return  # do nothing

    lines = report.split('\n')
    page = ""
    num = 0
    page_size = 48  # number of lines per page
    line_width = 160 # number of characters per line
    font_size = 4
    for line in lines:
        if (len(line) == 0):    # empty line
            line = "\n"
        s = 0
        while (len(line[s:]) > line_width):
            page += line[s:s + line_width]
            page += '\n'
            num += 1
            if (num == page_size):
                fig = plt.figure()
                plt.axis('off')
                page = page.replace('$', r'\$')  # escape '$'
                plt.text(0.0, 0.0, page, fontsize = font_size, ha = 'left', wrap = True)
                fig.savefig(p, format = 'pdf')
                plt.close(fig) # close figure
                page = ""  # reset for next page
                num = 0
            s += line_width
        if (s < len(line)):
            page += line[s:]
            page += '\n'
            num += 1
            if (num == page_size):
                fig = plt.figure()
                plt.axis('off')
                page = page.replace('$', r'\$')
                plt.text(0.0, 0.0, page, fontsize = font_size, ha = 'left', wrap = True)
                fig.savefig(p, format = 'pdf')
                plt.close(fig) # close figure
                page = ""  # reset for next page
                num = 0

    # don't forget the last page
    if (num != 0):
        fig = plt.figure()
        plt.axis('off')
        page = page.replace('$', r'\$')
        plt.text(0.0, 0.0, page, fontsize = font_size, ha = 'left', wrap = True)
        fig.savefig(p, format = 'pdf')
        plt.close(fig) # close figure

    return

def PlotStallChart(args, kernel, df2, stalls, p):
    """Chart the stall counts of one kernel, splitting a long kernel over pages.

    Creates the PDF on first use if p is None, and returns it either way.
    """
    ins_per_page = 100  # number of instructions to chart per page

    xlabels = []
    xticks = []
    for i, offset in enumerate(df2["Offset"]):
        xlabels.append(offset)
        xticks.append(i)

    df3 = df2[stalls]
    df3 = df3.reset_index()
    numpages = (df3.shape[0] + (ins_per_page - 1)) // ins_per_page # multiple charts if more than ins_per_page instructions are stalled
    for page in range(1, numpages + 1):
        first = ins_per_page * (page - 1)
        beyond = min([ins_per_page * page, df3.shape[0]])
        df4 = df3[first : beyond]
        if (df4.shape[0] > 1):	# draw line chart if there are at least 2 data points
            ax = df4.plot(y = stalls, kind = 'line', xlabel = "Offset", ylabel = "Events", fontsize = 6)
        else:	# draw bar chart otherwise
            ax = df4.plot(y = stalls, kind = 'bar', xlabel = "Offset", ylabel = "Events", fontsize = 6)
        ax.set_xticks(xticks[first : beyond], labels = xlabels[first : beyond], rotation = 90, fontsize = 4)
        plt.grid(visible = True, which = 'both', axis = 'y')
        plt.legend(loc = 'best', fontsize = 4)
        title = args.title + "\n(" + kernel + ")"
        if (numpages > 1):
            title += "(" + str(page) + "/" + str(numpages) + ")"
        plt.title(label = title, loc = 'center', fontsize = 8, wrap = True)
        plt.tight_layout()
        fig = ax.get_figure()
        if (p == None):
            p = pdf(args.output)
        fig.savefig(p, format = 'pdf')
        plt.close(fig)	# close figure to save memory

    return p

def AnalyzeStallMetrics(args, device_data_df, kernel, http = False):
    if ((http == True) and (kernel is None)):
        return None, None

    buf = None
    stall_analysis_report_out = None
    if ((http == False) and (args.report is not None)):
        stall_analysis_report_out = open(args.report, "w")

    df = device_data_df
    stalls = StallColumns(df)
    if (len(stalls) == 0):
        msg = "No stall metrics in the input data. Was it collected with unitrace --stall-sampling?"
        if (http == False):
            print(msg)
        if (stall_analysis_report_out is not None):
            stall_analysis_report_out.close()
        return msg, None

    ranges = KernelRanges(df)
    if (kernel is not None):
        kernels = [r for r in ranges if (r[0] == kernel)]
        if (len(kernels) == 0):
            # the name may still carry the kernel shape the trace shows
            pos = kernel.rfind('[')
            if (pos != -1):
                kernel = kernel[0:pos]
                kernels = [r for r in ranges if (r[0] == kernel)]
        if (len(kernels) == 0):
            msg = "No metric data for kernel " + kernel
            if (http == False):
                print(msg)
            if (stall_analysis_report_out is not None):
                stall_analysis_report_out.close()
            return msg, None
        ranges = kernels

    p = None
    tmpdir = None
    tmpfile = None
    if (http == True):
        tmpdir = tempfile.TemporaryDirectory()
        tmpfile = os.path.join(tmpdir.name, "stallchart.pdf")
        p = pdf(tmpfile)

    analyzed = 0
    for kname, pid, kernelid, start, stop in ranges:
        label = StallKernelLabel(kname, pid, kernelid)
        df2 = NonZeroStallRows(df.iloc[start : stop], stalls)   # data frame of the kernel of interest
        if (df2.shape[0] == 0):
            print("\nNo stall events for kernel " + label)
            continue

        p = PlotStallChart(args, label, df2, stalls, p)

        report = BuildStallReport(label, args, df2, stalls)
        if (stall_analysis_report_out is not None):
            print(report, file = stall_analysis_report_out)
        WriteOutStallReport(report, p)

        analyzed = analyzed + 1
        print("\nAnalyzed kernel " + label)

    if (p != None):
        p.close()

    if (http == True):
        if (analyzed > 0):
            buf = io.BytesIO()
            fp = open(tmpfile, "rb")
            buf.write(fp.read())
            fp.close()
            os.remove(tmpfile)
        tmpdir.cleanup()
    else:
        if (analyzed > 0):
            print("\nStall metric charts are stored in file " + args.output + " in PDF format")
        if (stall_analysis_report_out is not None):
            stall_analysis_report_out.close()
            if (analyzed > 0):
                print("Stall report is also stored in file " + args.report)

    if (analyzed == 0):
        msg = "No stall events in the input data"
        if (kernel is not None):
            msg = "No stall events for kernel " + kernel + " in the input data"
        return msg, None

    return None, buf

def PlotKernelInstancePerfMetrics(args, kernel, df, metric_sets_cleansed, throughputs_sets_cleansed, p):
    k = 0
    counting = True
    start = -1
    stop = -1
    instancefound = False

    for index, row in df.iterrows():
        if (pd.isna(row['Kernel']) == False):
            if ((row['Kernel'] == kernel) and (counting == True)):
                k = k + 1			# found kernel of interest
                counting = False		# set to false so this block will not enter again for this instance
                if (k == args.instance):	# found instance of interest
                    start = index
                    instancefound = True
        else:
            counting = True			# a new kernel and/or a new instance starts in the data frame
            if (instancefound == True):
                stop = index
                break

    if ((instancefound == True) and (stop == -1)):	# the instance of interest is at the end of data frame
        stop = df.shape[0]

    if (instancefound == False):
        print("No metric data for instance " + str(args.instance) + " of kernel " + kernel)
        return False, p

    df2 = df[start : stop]	# data frame of the instance of interest
    analyzed = False
    for metrics_cleansed, label in zip(metric_sets_cleansed, args.ylabel):
        df3 = df2[metrics_cleansed]

        if (df3.shape[0] > 0):
            if (df3.shape[0] > 1):
                ax = df3.plot(y = metrics_cleansed, kind = 'line', xlabel = args.xlabel, ylabel = label)
            else:
                ax = df3.plot(y = metrics_cleansed, kind = 'bar', xlabel = args.xlabel, ylabel = label)
            plt.grid(visible = True, which = 'both', axis = 'y')
            plt.legend(loc = 'best', fontsize = 4)
            plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
            plt.tight_layout()
            if (p == None):
                p = pdf(args.output)
            fig = ax.get_figure()
            fig.savefig(p, format = 'pdf')
            plt.close(fig)	# close figure to save memory
            analyzed = True
        else:
            break

    if (analyzed == True):
        for throughputs_cleansed in throughputs_sets_cleansed:
            df3 = df2[throughputs_cleansed]
            df3 = (df3.iloc[:, 1:]).div(df3.iloc[:, 0], axis = 0)

            if (df3.shape[0] > 0):
                if (df3.shape[0] > 1):
                    ax = df3.plot(y = throughputs_cleansed[1:], kind = 'line', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')
                else:
                    ax = df3.plot(y = throughputs_cleansed[1:], kind = 'bar', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')

                plt.grid(visible = True, which = 'both', axis = 'y')
                plt.legend(loc = 'best', fontsize = 4)
                plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                plt.tight_layout()
                if (p == None):
                    p = pdf(args.output)
                fig = ax.get_figure()
                fig.savefig(p, format = 'pdf')
                plt.close(fig)	# close figure to save memory

            else:
                break

    return analyzed, p

def AnalyzePerfMetrics(args, device_data_df):
    metric_sets_cleansed = []
    for metric_set in args.metrics:
        metrics = metric_set.split(sep = ',')
        metrics_cleansed = []
        for metric in metrics:
            me = metric.strip()			# strip off leading and trailing whitespaces
            if (me not in device_data_df.columns):
                print("No metric data for " + metric)
                return
            else:
                metrics_cleansed.append(me)
        metric_sets_cleansed.append(metrics_cleansed)

    throughputs_sets_cleansed = []
    if (args.throughput is not None):
        for throughput_set in args.throughput:
            throughputs = throughput_set.split(sep = ',')
            throughputs_cleansed = ['GpuTime[ns]']
            for throughput in throughputs:
                bw = throughput.strip()			# strip off leading and trailing whitespaces
                if (bw not in device_data_df.columns):
                    print("No throughput data for " + bw)
                    return
                else:
                    throughputs_cleansed.append(bw)
            throughputs_sets_cleansed.append(throughputs_cleansed)

    p = None
    if (args.kernel is None):
        if (args.instance < 1):
            # all kernels and all instances
            start = 0
            stop = -1
            kernel = ""
            for index, row in device_data_df.iterrows():
                if (pd.isna(row['Kernel']) == True):
                    stop = index	# current kernel instance ends

                    df2 = device_data_df[start : stop]	# data frame of the instance of interest
                    analyzed = False
                    for metrics_cleansed, label in zip(metric_sets_cleansed, args.ylabel):
                        df3 = df2[metrics_cleansed]
                        if (df3.shape[0] > 0):
                            if (df3.shape[0] > 1):
                                ax = df3.plot(y = metrics_cleansed, kind = 'line', xlabel = args.xlabel, ylabel = label)
                            else:
                                ax = df3.plot(y = metrics_cleansed, kind = 'bar', xlabel = args.xlabel, ylabel = label)

                            plt.grid(visible = True, which = 'both', axis = 'y')
                            plt.legend(loc = 'best', fontsize = 4)
                            plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                            plt.tight_layout()
                            if (p == None):
                                p = pdf(args.output)
                            fig = ax.get_figure()
                            fig.savefig(p, format = 'pdf')
                            plt.close(fig)	# close figure to save memory
                            analyzed = True
                        else:
                            break

                    if (analyzed == True):
                        for throughputs_cleansed in throughputs_sets_cleansed:
                            df3 = df2[throughputs_cleansed]
                            df3 = (df3.iloc[:, 1:]).div(df3.iloc[:, 0], axis = 0)

                            if (df3.shape[0] > 0):
                                if (df3.shape[0] > 1):
                                    ax = df3.plot(y = throughputs_cleansed[1:], kind = 'line', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')
                                else:
                                    ax = df3.plot(y = throughputs_cleansed[1:], kind = 'bar', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')

                                plt.grid(visible = True, which = 'both', axis = 'y')
                                plt.legend(loc = 'best', fontsize = 4)
                                plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                                plt.tight_layout()
                                if (p == None):
                                    p = pdf(args.output)
                                fig = ax.get_figure()
                                fig.savefig(p, format = 'pdf')
                                plt.close(fig)	# close figure to save memory
                                analyzed = True
                            else:
                                break

                        print("Analyzed kernel " + kernel)
                    else:
                        print("No samples for kernel " + kernel)

                    kernel = ""	# reset kernel name to empty
                else:
                    if (kernel == ""):
                        start = index	# a new kernel instance starts
                        kernel = row['Kernel']

        else:
            # specified instance of all kernels
            kernels = device_data_df['Kernel'].unique()
            for kernel in kernels:
                if (pd.isna(kernel) == True):
                    continue

                analyzed, p = PlotKernelInstancePerfMetrics(args, kernel, device_data_df, metric_sets_cleansed, throughputs_sets_cleansed, p)
                if (analyzed == True):
                    print("Analyzed kernel " + kernel)

    else:
        if (args.instance < 1):
            # specified kernel all instances
            counting = True
            start = -1
            stop = -1
            instance = 0
            for index, row in device_data_df.iterrows():
                if (pd.isna(row['Kernel']) == False):
                    if ((row['Kernel'] == args.kernel) and (counting == True)):
                        start = index			# found kernel of interest
                        counting = False		# set to false so this block will not enter again for this instance
                else:
                    if (counting == False):
                        # instance ends
                        stop = index
                        df2 = device_data_df[start : stop]	# data frame of the instance of interest
                        analyzed = False
                        for metrics_cleansed, label in zip(metric_sets_cleansed, args.ylabel):
                            df3 = df2[metrics_cleansed]

                            instance = instance + 1
                            if (df3.shape[0] > 0):
                                if (df3.shape[0] > 1):
                                    ax = df3.plot(y = metrics_cleansed, kind = 'line', xlabel = args.xlabel, ylabel = label)
                                else:
                                    ax = df3.plot(y = metrics_cleansed, kind = 'bar', xlabel = args.xlabel, ylabel = label)

                                plt.grid(visible = True, which = 'both', axis = 'y')
                                plt.legend(loc = 'best', fontsize = 4)
                                plt.title(label = args.title + "\n(" + args.kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                                plt.tight_layout()
                                if (p == None):
                                    p = pdf(args.output)
                                fig = ax.get_figure()
                                fig.savefig(p, format = 'pdf')
                                plt.close(fig)	# close figure to save memory
                                analyzed = True
                            else:
                                break

                        if (analyzed):
                            for throughputs_cleansed in throughputs_sets_cleansed:
                                df3 = df2[throughputs_cleansed]
                                df3 = (df3.iloc[:, 1:]).div(df3.iloc[:, 0], axis = 0)

                                if (df3.shape[0] > 0):
                                    if (df3.shape[0] > 1):
                                        ax = df3.plot(y = throughputs_cleansed[1:], kind = 'line', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')
                                    else:
                                        ax = df3.plot(y = throughputs_cleansed[1:], kind = 'bar', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')

                                    plt.grid(visible = True, which = 'both', axis = 'y')
                                    plt.legend(loc = 'best', fontsize = 4)
                                    plt.title(label = args.title + "\n(" + args.kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                                    plt.tight_layout()
                                    if (p == None):
                                        p = pdf(args.output)
                                    fig = ax.get_figure()
                                    fig.savefig(p, format = 'pdf')
                                    plt.close(fig)	# close figure to save memory
                                    analyzed = True
                                else:
                                    break

                            print("Analyzed instance " + str(instance) + " of kernel " + args.kernel)
                        else:
                            print("No samples for instance " + str(instance) + " of kernel " + args.kernel)

                        # continue scan for next instance
                        counting = True			# a new kernel and/or a new instance starts in the data frame
                        start = -1
                        stop = -1

            if ((start != -1) and (stop == -1)):	# the instance of interest is at the end of data frame
                stop = device_data_df.shape[0]

                df2 = device_data_df[start : stop]	# data frame of the instance of interest
                analyzed = False
                for metrics_cleansed, label in zip(metric_sets_cleansed, args.ylabel):
                    df3 = df2[metrics_cleansed]

                    instance = instance + 1
                    if (df3.shape[0] > 0):
                        if (df3.shape[0] > 1):
                            ax = df3.plot(y = metrics_cleansed, kind = 'line', xlabel = args.xlabel, ylabel = label)
                        else:
                            ax = df3.plot(y = metrics_cleansed, kind = 'bar', xlabel = args.xlabel, ylabel = label)

                        plt.grid(visible = True, which = 'both', axis = 'y')
                        plt.legend(loc = 'best', fontsize = 4)
                        plt.title(label = args.title + "\n(" + args.kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                        plt.tight_layout()
                        if (p == None):
                            p = pdf(args.output)
                        fig = ax.get_figure()
                        fig.savefig(p, format = 'pdf')
                        plt.close(fig)	# close figure to save memory
                        analyzed = True
                    else:
                        break

                if (analyzed == True):
                    for throughputs_cleansed in throughputs_sets_cleansed:
                        df3 = df2[throughputs_cleansed]
                        df3 = (df3.iloc[:, 1:]).div(df3.iloc[:, 0], axis = 0)

                        if (df3.shape[0] > 0):
                            if (df3.shape[0] > 1):
                                ax = df3.plot(y = throughputs_cleansed[1:], kind = 'line', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')
                            else:
                                ax = df3.plot(y = throughputs_cleansed[1:], kind = 'bar', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')

                            plt.grid(visible = True, which = 'both', axis = 'y')
                            plt.legend(loc = 'best', fontsize = 4)
                            plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                            plt.tight_layout()
                            if (p == None):
                                p = pdf(args.output)
                            fig = ax.get_figure()
                            fig.savefig(p, format = 'pdf')
                            plt.close(fig)	# close figure to save memory
                            analyzed = True
                        else:
                            break

                    print("Analyzed instance " + str(instance) + " of kernel " + args.kernel)
                else:
                    print("No samples for " + str(instance) + " of kernel " + args.kernel)

        else:
            # specified kernel specified instance
            analyzed, p = PlotKernelInstancePerfMetrics(args, args.kernel, device_data_df, metric_sets_cleansed, throughputs_sets_cleansed, p)
            if (analyzed == True):
                print("Performance metric chart for instance " + str(args.instance) + " of kernel " + args.kernel + " hsa been successfully generated")
            else:
                print("No performance data for instance " + str(args.instance) + " of kernel " + args.kernel)

    if (p != None):
        p.close()
        print("Performance metric charts are successfully generated in PDF format")
    else:
        print("No performance metric data for kernel or kernel instance")

def HttpAnalyzePerfMetrics(args, device_data_df, kname, instance):
    if (args.metrics is None):
        return None, None

    device_data_df = device_data_df.loc[device_data_df['GlobalInstanceId'] == float(instance)]
    if (device_data_df.shape[0] == 0):
        return None, None

    buf = None
    metric_sets_cleansed = []
    for metric_set in args.metrics:
        metrics = metric_set.split(sep = ',')
        metrics_cleansed = []
        for metric in metrics:
            me = metric.strip()			# strip off leading and trailing whitespaces
            if (me not in device_data_df.columns):
                msg = "No metric data for " + metric + ". Is the metric name correctly spelled?"
                return msg, None
            else:
                metrics_cleansed.append(me)
        metric_sets_cleansed.append(metrics_cleansed)

    throughputs_sets_cleansed = []
    if (args.throughput is not None):
        for throughput_set in args.throughput:
            throughputs = throughput_set.split(sep = ',')
            throughputs_cleansed = ['GpuTime[ns]']
            for throughput in throughputs:
                bw = throughput.strip()			# strip off leading and trailing whitespaces
                if (bw not in device_data_df.columns):
                    msg = "No metric data for " + bw + ". Is the metric name correctly spelled?"
                    return msg, None
                else:
                    throughputs_cleansed.append(bw)
            throughputs_sets_cleansed.append(throughputs_cleansed)

    p = None
    for metrics_cleansed, label in zip(metric_sets_cleansed, args.ylabel):
        df2 = device_data_df[metrics_cleansed]

        if (df2.shape[0] > 0):
            if (df2.shape[0] > 1):
                ax = df2.plot(y = metrics_cleansed, kind = 'line', xlabel = args.xlabel, ylabel = label)
            else:
                ax = df2.plot(y = metrics_cleansed, kind = 'bar', xlabel = args.xlabel, ylabel = label)
            plt.grid(visible = True, which = 'both', axis = 'y')
            plt.legend(loc = 'best', fontsize = 4)
            kernel = device_data_df.iloc[0]['Kernel']
            plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
            plt.tight_layout()

            fig = ax.get_figure()
            if (p == None):
                tmpdir = tempfile.TemporaryDirectory()
                tmpfile = os.path.join(tmpdir.name, "perfchart.pdf")
                p = pdf(tmpfile)
            fig.savefig(p, format = 'pdf')
            plt.close(fig)  # close figure to save memory
        else:
            break

    if (p != None):
        for throughputs_cleansed in throughputs_sets_cleansed:
            df2 = device_data_df[throughputs_cleansed]
            df2 = (df2.iloc[:, 1:]).div(df2.iloc[:, 0], axis = 0)
            if (df2.shape[0] > 0):
                if (df2.shape[0] > 1):
                    ax = df2.plot(y = throughputs_cleansed[1:], kind = 'line', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')
                else:
                    ax = df2.plot(y = throughputs_cleansed[1:], kind = 'bar', xlabel = args.xlabel, ylabel = 'Throughput(GB/s)')

                plt.grid(visible = True, which = 'both', axis = 'y')
                plt.legend(loc = 'best', fontsize = 4)
                plt.title(label = args.title + "\n(" + kernel + ")", loc = 'center', fontsize = 8, wrap = True)
                plt.tight_layout()
                fig = ax.get_figure()
                fig.savefig(p, format = 'pdf')
                plt.close(fig)	# close figure to save memory
            else:
                break

        p.close()
        buf = io.BytesIO()
        fp = open(tmpfile, "rb")
        buf.write(fp.read())
        fp.close()
        os.remove(tmpfile)
        tmpdir.cleanup()

    return None, buf


def GenerateSelfSignedCertificate(cert, key):
    # construct command to generate a self-signed certificate and private key
    command = ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-keyout", key, "-out", cert, "-days", "365", "-nodes", "-subj", "/CN=localhost"]

    try:
        subprocess.run(command, check = True)
        print(f"Self-signed certificate and private key have been generated.")
    except subprocess.CalledProcessError as e:
        print(f"Error occurred while generating self-signed certificate: {e}", file = sys.stderr)

def PerfMetricsHTTPServer(args, metrics_info):
    try:
        class PerfMetricsRequestHandler(BaseHTTPRequestHandler):
            def _send_invalid(self):
                self.send_header('Content-type', 'text/plain')
                self.end_headers()
                self.wfile.write(b'Invalid')

            def do_GET(self):
                if (self.client_address[0] != "127.0.0.1"):
                    return
                request = unquote(self.path).split("/")  # format is name/instance
                if (request[1] == "favicon.ico"): # ignore
                    return

                self.send_response(200)
                expected_request_params = 4 if args.result_dir else 3
                if (len(request) < expected_request_params):
                    self._send_invalid()
                    return

                kname = request[1].strip('"')   # strip leading and trailing double quotes
                instance = request[2]
                if ((instance == "") or (instance.isdigit() == False)):
                    self._send_invalid()
                    return

                device_id = None
                if args.result_dir:
                    device_id = request[3]
                    if ((device_id == "") or (device_id.isdigit() == False)):
                        self._send_invalid()
                        return

                if (metrics_info.is_eustall):   # strip kernel shape for stall sampling
                    pos = kname.rfind('[')
                    if (pos != -1):
                        kname = kname[0:pos]

                msg = None
                buf = None
                target_devices = []

                # for result_directory mode. device id is taken from request
                # and we can search the kernel instance in this device only (not for eustall case)
                if device_id is not None and not metrics_info.is_eustall:
                    device_info = metrics_info.get_device(int(device_id))
                    if device_info:
                        target_devices.append(device_info)
                else:
                    target_devices = list(metrics_info.devices.values())

                for device_info in target_devices:
                    device_data_df = device_info.get_device_data()
                    if (device_data_df is None) or (device_data_df.shape[0] == 0):
                        msg = f"No metric data for device {device_info.device_id}"
                        continue
                    if metrics_info.is_eustall:
                        msg, buf = AnalyzeStallMetrics(args, device_data_df, kname, http = True)
                    else:
                        msg, buf = HttpAnalyzePerfMetrics(args, device_data_df, kname, instance)

                    if (buf is not None):
                        buf.seek(0)
                        try:
                            self.send_header('Content-type', 'application/pdf')
                            self.end_headers()
                            self.wfile.write(buf.read())
                        except BrokenPipeError:
                            print("Try again please")
                            return
                        return
                    else:
                        if (msg is not None):
                            break

                self.send_header('Content-type', 'text/plain')
                self.end_headers()
                if (msg is not None):
                    self.wfile.write(bytes(msg, 'utf-8'))
                else:
                    self.wfile.write(b'No metric data collected or no metrics specified')

        # matplotlib does not like ThreadingHTTPServer
        httpd = HTTPServer(('localhost', 8000), PerfMetricsRequestHandler)

        if (args.https == True):
            cert = args.certificate
            key = args.key
            tmpdir = None
            if ((cert == None) and (key == None)):
                # generate a self signed certificate
                print("No certificate or private key is provided.")
                print("Generating a self-signed certificate...")
                tmpdir = tempfile.TemporaryDirectory()
                cert = os.path.join(tmpdir.name, "selfsigned.crt")
                key = os.path.join(tmpdir.name, "private.key")
                GenerateSelfSignedCertificate(cert, key)
            else:
                if (cert is None):
                    print("Certificate file is missing")
                    return
                if (key is None):
                    print("Private key file is missing")
                    return

                if (os.path.isfile(cert) == False):
                    print("Certificate file " + cert + " does not exist")
                    return
                if (os.path.isfile(key) == False):
                    print("Private key file " + key + " does not exist")
                    return

            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(cert, key)
            httpd.socket = context.wrap_socket(httpd.socket, server_side = True)
            if (tmpdir is not None):
                tmpdir.cleanup()

        httpd.serve_forever()

    except KeyboardInterrupt:
        print("Goodbye!")
        return

def main(args):
    try:
        metrics_info = MetricsInfo(input_path=args.input,
                                   is_result_directory=args.result_dir)
    except Exception as e:
        print(f"Error: {e}")
        return

    if (args.list == True):
        metrics_info.print_data(args.output)
        return

    metric_ylabel_match = True
    if (((args.metrics is not None) and (args.ylabel is None)) or ((args.metrics is None) and (args.ylabel is not None))):
        metric_ylabel_match = False
    else:
        if ((args.metrics is not None) and (args.ylabel is not None) and (len(args.metrics) != len(args.ylabel))):
            metric_ylabel_match = False

    if (metric_ylabel_match == False):
        print("Number of metric sets does not match number of Y labels")
        return

    if ((args.https == True) and (args.http == True)):
        print("Options -p and -q cannot be used at the same time")
        return

    if ((args.https == True) or (args.http == True)):
        print("No output to file")
        print("Options -o/--output, -r/--report, -k/--kernel, -i/--instance and -d/--device are ignored if they are present")
        PerfMetricsHTTPServer(args, metrics_info)
        return

    if (args.output is None):
        print("Error: -o/--output is missing")
        return

    device_info = metrics_info.get_device(args.device)
    if device_info is None:
        print(f"Error: Device {args.device} not found in metrics data")
        return

    device_data_df = device_info.get_device_data()
    if (device_data_df is None) or (device_data_df.shape[0] == 0):
        print(f"No metric data for device {device_info.device_id}")
        return

    # The selected device's format, not is_eustall, which is the lowest-numbered device's.
    if (device_info.kind != "stall"):
        if (args.metrics is None):
            print("-m option is missing")
            return
        if (args.ylabel is None):
            print("-y option is missing")
            return
        if (len(args.metrics) != len(args.ylabel)):
            print("Numbers of -m options and -y options do not match")
            return
        AnalyzePerfMetrics(args, device_data_df)
    else:
        AnalyzeStallMetrics(args, device_data_df, args.kernel)

if __name__== "__main__":
    main(ParseArguments(sys.argv[1:]))  # skip sys.argv[0]
