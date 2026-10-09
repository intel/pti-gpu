#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
"""Validate the stall-sampling output of the gemm_stall_sampling test.

Checks that a metrics file was produced with the columns this script reads, that the
Source column attributes each kernel its own source lines, and that the Offset and
Dependency columns are internally consistent. More checks (per-scenario kernel
coverage) are meant to be added as functions in CHECKS below, without restructuring
anything else.

Only the metrics file is checked; see CHECKS for why the .asm files are not.

The report is CSV, one block per kernel (see ComputeStallMetrics): a
"Kernel,Pid,KernelId" line, that kernel's identity, its column header, then its
rows. Parsed with the csv module rather than by splitting on the separator, because
a comma inside a quoted cell is data - instruction syntax is full of them.

The source-attribution checks deliberately do not fail when the report carries no
source information at all: source line mapping is an optional capability (it needs
debug info in the module binary, which unitrace's own line-table reader
(source_resolver.h) decodes), and a build without it is expected to report NA rather than to fail
this test. What they do catch is source information that is present but wrong -
some kernels attributed and others not, or two kernels handed the same lines.

Every content check follows that shape, because which instructions get sampled is
nondeterministic: such a check may only constrain values the report actually
carries, never require a particular row, kernel or column to be populated. "The
Dependency column is empty everywhere" is therefore reported as a count, not a
failure - a run where sampling happened to land only on instructions with no
tracked producer is a valid run.

That shape has one cost, and check_header and check_has_data_rows exist to pay it:
a check that only constrains what is there passes when nothing is there. Every
content check below locates its column by index within a block's rows, so a report
that renames a column, drops one, or stops writing blocks does not fail them -
read_blocks recognises no block, iter_data_rows yields nothing, and all of them
report "nothing to check" and pass. The two structural checks run first and cannot
pass vacuously, so that failure is theirs to catch.

run_test.py invokes this through its -e/--extra_test_prog hook and passes the
output files it discovered as positional arguments. Those are NOT relied on for
finding the metrics file: the hook collects file names by grepping unitrace's
stderr for the literal "is stored in", but stall sampling announces its output
as "Device metrics are stored in ..." ("are", not "is"), so the hook finds
nothing and passes an empty list. This script therefore locates the metrics
files itself and treats "no metrics file found" as a failure - otherwise an
empty result would silently pass and the test would be worthless.
"""

import argparse
import collections
import csv
import glob
import os
import re
import sys

# Unitrace derives the metrics file name from the -o value by inserting
# ".metrics" before the extension (see LegacyLoggerFactory::GetMetricsFileName),
# giving e.g. output_stall_sampling_gemm_stall_sampling_scenario1.metrics.12345.txt
METRICS_GLOB = "*.metrics.*"

# The kernel that calls a device function defined in another source file (see
# gemm_helper.h). Its rows are the only ones expected to name more than one file.
MULTI_FILE_KERNEL = "GEMM_Call"

# The line that opens a kernel's block, and the header of the identity line under it.
# Matched exactly, so a report that stops writing blocks fails check_header rather than
# quietly reducing every content check to "nothing to check".
KERNEL_BLOCK_HEADER = ["Kernel", "Pid", "KernelId"]

# The identity line itself: the kernel, then the two values that name its .asm file
# (see LoggerFactory::GetDisasmFileName), which is what a row is traced back through.
KERNEL_FIELD = 0
PID_FIELD = 1
KERNEL_ID_FIELD = 2
IDENTITY_FIELDS = 3

# Column layout of a stall-sampling row within a block (see ComputeStallMetrics):
#   Source  Offset  Instruction  Dependency  <one column per stall metric>
SOURCE_COLUMN = 0
OFFSET_COLUMN = 1
INSTRUCTION_COLUMN = 2
DEPENDS_ON_COLUMN = 3
MIN_COLUMNS = 4

# The header names for those columns, in order.
EXPECTED_COLUMNS = ["Source", "Offset", "Instruction", "Dependency"]

# What an unattributed row carries in the Source column. Also what a row gets when the
# DWARF row covering it carried no source location at all - line 0, i.e. compiler-generated
# code - so this value means "no source line", not only "no debug info".
NO_SOURCE = "NA"

# What a row with no tracked producers carries in the Dependency column.
NO_DEPENDENCIES = "NA"

# Gen/Xe instructions are 16 bytes, or 8 when compacted, so every instruction
# address in a kernel is a multiple of 8.
INSTRUCTION_ALIGNMENT = 8

# One Dependency cell holds "<offset> (<source>)[: <text>]" per producer, joined by
# " | " (FormatDependencies). Instruction syntax contains "|" too - the channel
# offset in "(8|M0)" - so the separator is only recognised when what follows starts
# like a new entry, i.e. an offset followed by a space, a colon or end of cell.
DEPENDS_ON_SPLIT_RE = re.compile(r" \| (?=0x[0-9a-fA-F]+(?:[ :]|$))")

# One such entry, taken apart. The source parenthetical is present only when the
# producer had one, and the ": <text>" only when the producer was found in the
# disassembly at all.
DEPENDS_ON_ENTRY_RE = re.compile(
    r"^(?P<offset>0x[0-9a-fA-F]+)"
    r"(?: \((?P<source>[^)]*)\))?"
    r"(?:: (?P<text>.*))?$")

# One kernel's block. kernel, pid and kernel_id are what its identity line said, the
# two numbers parsed, or None where it said nothing usable there; identity and columns
# are the raw lines, kept so that check_header can quote what was wrong with them
# (columns is None when the block ended before its column header). rows holds only the
# lines wide enough to read a column out of, which drops the blank line between blocks.
KernelBlock = collections.namedtuple(
    "KernelBlock", ["kernel", "pid", "kernel_id", "identity", "columns", "rows"])


def metrics_glob_from_cmd(cmd):
    """Derive the exact metrics file pattern from the unitrace command line.

    run_test.py passes the command it ran after --cmd, which contains
    "-o <dir>/output_stall_sampling_<test>.txt". Unitrace turns that into
    "<same>.metrics.<pid>.txt" (LegacyLoggerFactory::GetMetricsFileName inserts
    ".metrics" before the extension).

    Deriving the pattern from -o matters because all scenarios write into the
    same results directory: a bare "*.metrics.*" glob would also match files left
    by other scenarios, so a run that produced nothing could pass on a previous
    run's output. Returns None if -o is absent, leaving the caller on the broad
    glob.
    """
    if not cmd:
        return None
    for i, arg in enumerate(cmd):
        if arg == "-o" and i + 1 < len(cmd):
            out_file = os.path.basename(cmd[i + 1])
            pos = out_file.rfind(".")
            if pos == -1:
                return out_file + ".metrics.*"
            return out_file[:pos] + ".metrics.*"
    return None


def results_dir_from_cmd(cmd):
    """Directory of the unitrace -o argument, or None if -o is absent."""
    if not cmd:
        return None
    for i, arg in enumerate(cmd):
        if arg == "-o" and i + 1 < len(cmd):
            return os.path.dirname(os.path.abspath(cmd[i + 1]))
    return None


def find_metrics_files(candidates, search_dirs, pattern):
    """Collect metrics files from the passed-in list and by searching directories.

    Both sources are used: whatever run_test.py managed to pass is honoured, and
    the directory search covers the stall-sampling case the hook misses.
    """
    found = []

    for path in candidates:
        if ".metrics." in os.path.basename(path) and os.path.isfile(path):
            found.append(path)

    for directory in search_dirs:
        if not directory or not os.path.isdir(directory):
            continue
        for path in glob.glob(os.path.join(directory, pattern)):
            if os.path.isfile(path):
                found.append(path)
        # Also one level down. This does not cover --result-dir, which names its
        # metrics files metrics/metrics_<n>.csv rather than <-o name>.metrics.<pid>.
        for path in glob.glob(os.path.join(directory, "*", pattern)):
            if os.path.isfile(path):
                found.append(path)

    # Preserve discovery order while removing duplicates, so the report reads
    # predictably when a file is both passed in and found by the search.
    unique = []
    seen = set()
    for path in found:
        key = os.path.normcase(os.path.abspath(path))
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique


def read_blocks(path):
    """Parse the report into its per-kernel blocks, in file order.

    One place decides what a block and a data row are, so every check below agrees on
    it. A block opens with KERNEL_BLOCK_HEADER; the next row is its identity and the
    one after that its column header, whatever they turn out to say - a malformed one
    is kept rather than dropped so that check_header can report what was wrong with it.
    Anything before the first block header is skipped, which is the blank lines and the
    "=== Device #N Metrics ===" banner.

    Returns a list of KernelBlock. pid and kernel_id are None when the identity line
    did not carry integers there.
    """
    blocks = []
    opened = False
    identity = []
    columns = None
    rows = []
    # What the block just opened is still waiting for: its identity line, then its
    # column header. None once both have been read and the rest is rows.
    pending = None

    def flush():
        if not opened:
            return
        kernel = identity[KERNEL_FIELD] if len(identity) > KERNEL_FIELD else None
        values = []
        for index in (PID_FIELD, KERNEL_ID_FIELD):
            text = identity[index] if len(identity) > index else ""
            values.append(int(text) if text.isdigit() else None)
        blocks.append(KernelBlock(kernel, values[0], values[1], identity, columns, rows))

    with open(path, "r", errors="replace", newline="") as handle:
        for fields in csv.reader(handle):
            if fields == KERNEL_BLOCK_HEADER:
                flush()
                opened, identity, columns, rows, pending = True, [], None, [], "identity"
                continue
            if pending == "identity":
                identity, pending = fields, "columns"
                continue
            if pending == "columns":
                columns, pending = fields, None
                continue
            if opened and len(fields) >= MIN_COLUMNS:
                rows.append(fields)
    flush()
    return blocks


_BLOCKS_CACHE = {}


def kernel_blocks(path):
    """read_blocks, parsed once per file however many checks ask for it."""
    key = os.path.normcase(os.path.abspath(path))
    if key not in _BLOCKS_CACHE:
        _BLOCKS_CACHE[key] = read_blocks(path)
    return _BLOCKS_CACHE[key]


def block_label(block):
    """How a failure message names a block: the kernel, plus what tells it apart from a
    same-named one and points at its .asm file."""
    if block.pid is None or block.kernel_id is None:
        return block.kernel or "<unnamed kernel>"
    return f"{block.kernel} (pid {block.pid}, kernel id {block.kernel_id})"


def check_header(path):
    """Every block must carry the identity and the columns the checks below read.

    The one check here that cannot pass vacuously, and so the one that catches a
    report whose columns were renamed or dropped, or that stopped writing blocks
    altogether. Runs first for that reason: without it, such a report fails nothing.

    Returns (passed, message).
    """
    blocks = kernel_blocks(path)
    if not blocks:
        return False, f"no block header (no {','.join(KERNEL_BLOCK_HEADER)} line)"
    stall_columns = None
    for block in blocks:
        if len(block.identity) != IDENTITY_FIELDS:
            return False, (f"identity line has {len(block.identity)} field(s), need "
                           f"{IDENTITY_FIELDS}: {block.identity}")
        if block.pid is None or block.kernel_id is None:
            return False, f"identity line has a non-integer Pid or KernelId: {block.identity}"
        columns = block.columns or []
        if len(columns) < MIN_COLUMNS:
            return False, (f"{block_label(block)} has a {len(columns)}-column header, need at "
                           f"least {MIN_COLUMNS}: {columns}")
        if columns[:MIN_COLUMNS] != EXPECTED_COLUMNS:
            return False, (f"{block_label(block)} header columns are {columns[:MIN_COLUMNS]}, "
                           f"expected {EXPECTED_COLUMNS}")
        # The rest are the stall metrics, whose names and number are the metric group's,
        # and so the same for every block of the report.
        if stall_columns is None:
            stall_columns = columns[MIN_COLUMNS:]
        elif columns[MIN_COLUMNS:] != stall_columns:
            return False, (f"{block_label(block)} reports stall columns "
                           f"{columns[MIN_COLUMNS:]}, expected {stall_columns}")
    return True, (f"{len(blocks)} block(s), each {', '.join(EXPECTED_COLUMNS)} + "
                  f"{len(stall_columns)} stall column(s)")


def split_source(value):
    """Split a Source cell, "<file>:<line>[:<column>]", into (file, position).

    The file name is reduced to its base name on purpose. One report can spell the
    same file two ways: a line table can name one file with its directory and another
    as the compiler wrote it, so "/where/it/was/built/gemm_multi.cl:36" and "gemm_multi.cl:36" can both
    appear and mean the same line. Comparing base names sees through that, and
    nothing here needs the directory.

    Returns (None, None) if the cell does not look like a source location.
    """
    parts = value.split(":")
    position = []
    # Line, and column if present, taken from the right. At most two, so a
    # directory that happens to be all digits is not read as a line number.
    while len(parts) > 1 and len(position) < 2 and parts[-1].isdigit():
        position.insert(0, parts.pop())
    if not position:
        return None, None
    return os.path.basename(":".join(parts)), ":".join(position)


def iter_data_rows(path):
    """Yield (kernel_name, fields) for each stall-sample row, in file order.

    The kernel name is the one its block's identity line carried, already unquoted by
    the csv reader. Use kernel_blocks() instead where a check needs to keep a block's
    rows apart from a same-named block's.
    """
    for block in kernel_blocks(path):
        for fields in block.rows:
            yield block.kernel, fields


def check_has_data_rows(path):
    """There must be at least one block, and every block must have rows under it.

    A block with no rows cannot happen - a block exists only because a sample was
    attributed to that kernel, and that sample's row went into it - so one means the
    writer emitted a header it had nothing to put under.

    No block at all is the case worth being deliberate about, and it is a failure here
    for the same reason the header-with-no-rows it replaces was: a run whose samples all
    fell in code no .kprops record covers writes no block, and treating that as a pass
    would make this check - and with it every content check, which has nothing to
    constrain - satisfied by a file that says nothing about any kernel. Stall sampling is
    per device, so that can happen when another process is busy on the same GPU;
    test_unitrace.py runs one test at a time, which is what keeps it rare.

    Returns (passed, message).
    """
    blocks = kernel_blocks(path)
    if not blocks:
        return False, "no kernel blocks in the report"
    empty = [block_label(block) for block in blocks if not block.rows]
    if empty:
        return False, f"block(s) with no data rows under them: {', '.join(empty)}"
    rows = sum(len(block.rows) for block in blocks)
    return True, f"{rows} row(s) across {len(blocks)} kernel block(s)"


def read_kernel_sources(path):
    """Map kernel name -> set of (file, position) pairs its rows reported.

    Rows with no source location contribute nothing to the set, so a kernel that was
    sampled but never attributed ends up with an empty set - which is what tells it
    apart from a kernel that was not sampled at all and so is absent entirely.

    Kernels are keyed by name, which merges the same name in two modules (scenario 3
    has GEMM in both gemm_single and gemm_multi) into one entry. Harmless for the source
    checks - they only ever ask which files a name was attributed, and both modules
    compile the same file - and deliberate, because check_distinct_sources and
    check_kernel_source_files both name the kernels they are about. A check that needs
    the two apart has kernel_blocks(), whose blocks carry the pid and kernel id.
    """
    sources = {}
    for kernel, fields in iter_data_rows(path):
        entry = sources.setdefault(kernel, set())
        if fields[SOURCE_COLUMN] == NO_SOURCE:
            continue
        name, position = split_source(fields[SOURCE_COLUMN])
        if name is not None:
            entry.add((name, position))
    return sources


_SOURCE_CACHE = {}


def kernel_sources(path):
    """read_kernel_sources, parsed once per file however many checks ask for it."""
    key = os.path.normcase(os.path.abspath(path))
    if key not in _SOURCE_CACHE:
        _SOURCE_CACHE[key] = read_kernel_sources(path)
    return _SOURCE_CACHE[key]


def check_source_attribution(path):
    """Either every sampled kernel got source lines, or none of them did.

    A kernel whose rows carry no source location while its siblings' do is the
    signature of source lines reaching the wrong kernel: a multi-kernel module's
    line-number program holds one unit per kernel, and a reader that mis-associates
    units leaves some kernels with nothing and gives others two kernels' worth.

    "None of them did" is a pass, not a failure - see the module docstring.
    """
    sources = kernel_sources(path)
    if not sources:
        return True, "no kernel rows to attribute"
    attributed = sorted(name for name, lines in sources.items() if lines)
    unattributed = sorted(name for name, lines in sources.items() if not lines)
    if not attributed:
        return True, f"no source information at all ({len(sources)} kernels, every row NA)"
    if unattributed:
        return False, (f"{', '.join(attributed)} got source lines but "
                       f"{', '.join(unattributed)} did not")
    return True, f"{len(attributed)} kernel(s), all with source lines"


def check_distinct_sources(path):
    """No two kernels may report an identical set of source lines.

    Cheap, GPU independent, and aimed at the failure that looks most ordinary in the
    report: every kernel of a module handed the first kernel's line table. Each
    kernel of gemm_multi.cl covers its own region of the file, so identical sets
    cannot arise by accident, however the sampling happened to land.
    """
    attributed = {name: lines for name, lines in kernel_sources(path).items() if lines}
    if len(attributed) < 2:
        return True, f"{len(attributed)} attributed kernel(s), nothing to compare"
    owner_of = {}
    for kernel in sorted(attributed):
        lines = frozenset(attributed[kernel])
        if lines in owner_of:
            return False, (f"{kernel} and {owner_of[lines]} report the same "
                           f"{len(lines)} source line(s)")
        owner_of[lines] = kernel
    return True, f"{len(attributed)} kernels, all with distinct source lines"


def parse_offset(value):
    """Parse an Offset cell, "0x" plus hex digits (HexOffset writes at least 8).

    Returns None if the cell is not one, which a check reports rather than crashes on.
    """
    if not value.startswith("0x") or len(value) <= 2:
        return None
    try:
        return int(value[2:], 16)
    except ValueError:
        return None


def parse_depends_on(value):
    """Split a Dependency cell into a list of (offset_text, producer_text) pairs.

    producer_text is None when the entry carries no ": <text>" part, which happens
    when the producer offset named no instruction in the disassembly. An entry that
    does not match the expected shape at all yields (raw_entry, None) with an
    offset_text that parse_offset will reject, so the caller reports it as malformed
    instead of silently skipping it.
    """
    if value == NO_DEPENDENCIES or not value:
        return []
    entries = []
    for entry in DEPENDS_ON_SPLIT_RE.split(value):
        match = DEPENDS_ON_ENTRY_RE.match(entry)
        if match is None:
            entries.append((entry, None))
            continue
        entries.append((match.group("offset"), match.group("text")))
    return entries


def check_offsets_aligned(path):
    """Every Offset, and every offset named in Dependency, must be 8-byte aligned.

    The Offset column is 8-aligned by construction - a raw stall sample carries the
    address in 8-byte units and ComputeStallMetrics shifts it left by 3 - so what this
    really pins down is that the kernel base subtracted from it is aligned too, and,
    more usefully, the Dependency offsets. Those are not sample addresses: they are
    read out of the disassembly's own instruction table, so a mis-sized or
    mis-attributed instruction record surfaces here as an offset landing mid-encoding.

    Cannot depend on which instructions were sampled: it only inspects offsets the
    report already contains.
    """
    checked = 0
    for block in kernel_blocks(path):
        for fields in block.rows:
            cells = [("Offset", fields[OFFSET_COLUMN])]
            cells += [("Dependency", text)
                      for text, _ in parse_depends_on(fields[DEPENDS_ON_COLUMN])]
            for column, text in cells:
                offset = parse_offset(text)
                if offset is None:
                    return False, (f"{block_label(block)} has an unparsable {column} "
                                   f"value '{text}'")
                if offset % INSTRUCTION_ALIGNMENT != 0:
                    return False, (f"{block_label(block)} {column} {text} is not a multiple of "
                                   f"{INSTRUCTION_ALIGNMENT}")
                checked += 1
    if not checked:
        return True, "no sampled rows to check"
    return True, f"{checked} offset(s), all {INSTRUCTION_ALIGNMENT}-byte aligned"


def check_depends_on_wellformed(path):
    """Every Dependency entry must parse, and must quote the right producer.

    Where an entry names an offset that another row of the same block also reports as
    its own Offset, the instruction text quoted in the entry must be that row's
    Instruction text. The two come from separate lookups into the same table (the
    row's own, and FormatDependencies' lookup of the producer), so a disagreement
    means the table maps one offset to two different instructions.

    Compared within a block and never across two, which is what makes it safe when one
    name belongs to two kernels. Scenario 3 launches a GEMM from gemm_single.cl and
    another from gemm_multi.cl: the two are separate compilations of the same algorithm,
    so offset 0x1b8 holds a "macl" in both but with a different destination register, and
    comparing one against the other would make correct output look self-contradictory.

    An entry naming its own row is not an error. A loop counter reads the register it
    writes - "add r3.8:d r3.8:d 1:w" - so IGA's defs analysis lists the instruction as
    its own producer, and that back edge is the dependency a stall on the increment is
    explained by.

    Requires no particular row to have been sampled: producers are frequently not
    sampled themselves, so the text comparison is only made for offsets that do
    appear, and the count of rows carrying dependencies is reported rather than
    required to be non-zero.
    """
    rows_with_deps = 0
    total_deps = 0
    for block in kernel_blocks(path):
        # Offset -> Instruction text, for this kernel's sampled rows only.
        text_at = {}
        for fields in block.rows:
            offset = parse_offset(fields[OFFSET_COLUMN])
            if offset is not None:
                text_at[offset] = fields[INSTRUCTION_COLUMN]

        for fields in block.rows:
            entries = parse_depends_on(fields[DEPENDS_ON_COLUMN])
            if not entries:
                continue
            rows_with_deps += 1
            total_deps += len(entries)
            for offset_text, producer_text in entries:
                producer_offset = parse_offset(offset_text)
                if producer_offset is None:
                    return False, (f"{block_label(block)} has an unparsable Dependency "
                                   f"entry '{offset_text}'")
                expected = text_at.get(producer_offset)
                if expected is None or producer_text is None:
                    continue  # Producer not sampled, or quoted without its text.
                if producer_text != expected:
                    return False, (f"{block_label(block)} offset {offset_text} is reported as "
                                   f"'{expected}' in its own row but quoted as "
                                   f"'{producer_text}' as a producer")
    if not rows_with_deps:
        return True, "no row reported dependencies"
    return True, f"{total_deps} dependency/ies over {rows_with_deps} row(s), all well formed"


def check_multi_file_kernel(path):
    """GEMM_Call's rows must name at least two source files.

    It is the one kernel whose accumulation step is a call into another file, so its
    line program covers gemm_helper.h as well as gemm_multi.cl. One file means
    either the device compiler inlined the call away or the reader dropped the rows
    belonging to the called file, and in both cases the multi-file coverage this
    kernel exists for is gone without anything else looking wrong.

    Scenarios that do not launch GEMM_Call have nothing to check here.
    """
    lines = kernel_sources(path).get(MULTI_FILE_KERNEL)
    if not lines:
        return True, f"{MULTI_FILE_KERNEL} reported no source lines here"
    files = sorted({name for name, _ in lines})
    if len(files) < 2:
        return False, (f"{MULTI_FILE_KERNEL} names only {files[0]}; expected the "
                       "called file as well")
    return True, f"{MULTI_FILE_KERNEL} names {', '.join(files)}"


# Checks run against every metrics file found. Each entry is (name, function),
# where the function takes a path and returns (passed, message). Add new checks
# here.
#
# Only the metrics file is checked. The per-kernel .asm files the run also writes are
# deliberately left alone: their layout is not settled yet, and a test pinning it would
# have to be rewritten with every change to it.
#
# header and has-data-rows come first: they are the two that cannot pass when the
# report is unreadable, so they diagnose it before the rest report "nothing to check".
CHECKS = [
    ("header", check_header),
    ("has-data-rows", check_has_data_rows),
    ("source-attribution", check_source_attribution),
    ("distinct-sources", check_distinct_sources),
    ("multi-file-kernel", check_multi_file_kernel),
    ("offsets-aligned", check_offsets_aligned),
    ("depends-on-wellformed", check_depends_on_wellformed),
]


def main():
    parser = argparse.ArgumentParser(
        description="Validate gemm_stall_sampling stall-sampling output.")
    parser.add_argument('output_files', nargs='*', help='output files from run_test.py')
    parser.add_argument('--results-dir', type=str, default=None,
                        help='directory to search for metrics files')
    # run_test.py appends the unitrace command it ran; the -o and the flags in it
    # tell us which file to look for and whether metrics were collected at all.
    parser.add_argument('--cmd', type=str, nargs=argparse.REMAINDER,
                        help='unitrace command that was run')
    args = parser.parse_args()

    # There is nothing to validate if the run never sampled. Only bail out when
    # the command line is known: with no --cmd, a missing file is still a failure.
    if args.cmd and "--stall-sampling" not in args.cmd:
        print("[INFO] Not a stall-sampling run, nothing to check.")
        return 0

    search_dirs = []
    if args.results_dir:
        search_dirs.append(args.results_dir)
    else:
        # run_test.py writes the results next to the unitrace -o file, so
        # search the directory of the -o path in the command it ran.
        results_dir = results_dir_from_cmd(args.cmd)
        if results_dir:
            search_dirs.append(results_dir)
    # Also look beside any file that was passed in, in case the layout differs.
    for path in args.output_files:
        parent = os.path.dirname(os.path.abspath(path))
        if parent not in search_dirs:
            search_dirs.append(parent)

    # Prefer the exact pattern derived from the unitrace -o argument; fall back to
    # the broad glob only when it cannot be determined.
    pattern = metrics_glob_from_cmd(args.cmd) or METRICS_GLOB
    print(f"[INFO] Looking for metrics files matching '{pattern}'")

    metrics_files = find_metrics_files(args.output_files, search_dirs, pattern)

    if not metrics_files:
        print("[ERROR] No metrics file found. Searched:", file=sys.stderr)
        for directory in search_dirs:
            print(f"[ERROR]   {directory} (pattern {pattern})", file=sys.stderr)
        if args.output_files:
            print("[ERROR] Files passed in by run_test.py:", file=sys.stderr)
            for path in args.output_files:
                print(f"[ERROR]   {path}", file=sys.stderr)
        else:
            print("[ERROR] run_test.py passed no output files.", file=sys.stderr)
        return 1

    failed = False
    for path in metrics_files:
        print(f"[INFO] Checking {path}")
        for name, check in CHECKS:
            try:
                passed, message = check(path)
            except OSError as e:
                passed, message = False, f"could not read file: {e}"
            if passed:
                print(f"[INFO]   {name}: OK ({message})")
            else:
                print(f"[ERROR]  {name}: FAILED ({message})", file=sys.stderr)
                failed = True

    if failed:
        return 1
    print(f"[INFO] All checks passed on {len(metrics_files)} metrics file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
