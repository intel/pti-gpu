# Fuzz Testing for Unitrace (SDL505)

Fuzz harnesses and campaign scripts for SDL task SDL505 - Perform Fuzz Testing
(UNITRACEI-135). The evidence report and the findings register are not committed:
`make_evidence.py` writes them to `evidence/`, and `tar -czf SDL505_evidence.tar.gz
evidence` packs them for the SDL upload.

## What is fuzzed

Every harness calls the code unitrace ships, not a copy. The parsers of files
unitrace reads back from disk, and the helpers that write names into JSON or
check numbers from the environment and the driver, are functions of their own
in the file that uses them (`ze_metrics.h`, `ze_collector.h`, `utils_ze.h`,
`utils_host.h`, `chromelogger.h`, `unitrace.cc`), free of Level Zero calls. The
harnesses include those files; `UNITRACE_FUZZ` leaves out `unitrace.cc`'s
`main()`. The one exception: `fuzz_metric_samples` repeats the two `ze_metrics.h`
loops that read the values once `CalculatedSamplesFit` accepted the counts, since
they call Level Zero.

| Target | Untrusted input | Code |
|--------|-----------------|------|
| `fuzz_elf64` | Module native binary from the GPU driver (ELF64/ZeBin, DWARF `.debug_line`) | `elf64_reader.h`, `kernel_extractor.h`, `source_resolver.h` |
| `fuzz_metric_query` | `.metrics.<pid>.q` in `UNITRACE_DataDir` | `ze_metrics.h` `ReadMetricQueryRecord` |
| `fuzz_stall_sampling` | `.t` sampling segments in `UNITRACE_DataDir` | `ze_metrics.h` `ReadMetricSegment` |
| `fuzz_kernel_props` | `.kprops` / `.ktime` in `UNITRACE_DataDir` | `ze_metrics.h` `ReadKernelPropsRecord`, `ReadKernelTimeRecord` |
| `fuzz_cli_args` | Command line | `cli_options.h` `ParseArgs`, `JsonEscape` |
| `fuzz_kernel_filter` | `--include/--exclude-kernels[-file]` | `ze_collector.h` `ParseFilterList` |
| `fuzz_sample_lists` | `--devices/--ranks-to-sample`, `PMI_SIZE`, `PMI_RANK`, ... | `utils_ze.h` `ParseDeviceList`, `ParseRankList` |
| `fuzz_kmd_trace` | bpftrace output for `--chrome-kmd-logging` | `unitrace.cc` `ReadKmdEvent`, `KmdEventToJson` |
| `fuzz_analyzeperfmetrics` | Metric CSV | `analyzeperfmetrics.py` (also the `perfdashboard.py` read path) |
| `fuzz_mergetrace` | Chrome JSON / Perfetto traces | `mergetrace.py` |
| `fuzz_summary` | Tar archive of summaries | `summary.py` `read_summary_tar` |
| `fuzz_categorize` | Category schema INI | `categorize.py` `build_tree_from_ini` |
| `fuzz_roofline_csv` | Device config and metric CSV | `roofline_libs.py` |
| `fuzz_trace_json` | Kernel, ITT and device names and the command line written to the trace and config log | `utils_host.h` `JsonString`, `chromelogger.h` `JsonNameField`, `unitrace.cc` `StringifyJsonArray` |
| `fuzz_env_numbers` | `UNITRACE_DeviceId`, `PTI_DEVICE_ID`, `--chrome-event-buffer-size`, ... | `utils_ze.h` `ParseDeviceIndex`, `DevicesStringToSet`, `utils_host.h` `ParseChromeEventBufferSize` |
| `fuzz_metric_samples` | Counts the driver returns with calculated metric values (in place of a Level Zero mock) | `ze_metrics.h` `CalculatedSamplesFit`, `UnwrapTimestamp` |
| `fuzz_metrics_server` | Requests to the localhost metrics server, `-f` options file | `analyzeperfmetrics.py` `ParseMetricsRequest`, `ReadOptionsFromFile` |
| `fuzz_perfdashboard_upload` | CSV uploaded to the counter dashboard | `perfdashboard.py` `upload_csv_input1` |

Besides crashes, hangs and memory exhaustion, some harnesses check output:
`fuzz_kmd_trace`, `fuzz_cli_args` and `fuzz_trace_json` require valid JSON (`cpp/json_check.h`),
`fuzz_env_numbers` requires an accepted number to be the one written,
`fuzz_mergetrace` requires a successful merge to be valid JSON. A Python harness
lists the exceptions that are a clean rejection of malformed input; any other
exception is a finding.

## Build

C++: the targets build with unitrace (clang with libFuzzer; the Level Zero headers
unitrace fetches, no GPU needed). Any clang that links libFuzzer works, including
the one oneAPI ships (`$CMPLR_ROOT/bin/compiler/clang++`); test_unitrace.py picks it
by itself when there is no clang on PATH. The binaries land in `build-fuzz/test/fuzz/cpp`:

```sh
cmake -S tools/unitrace -B build-fuzz -DUNITRACE_BUILD_FUZZ=ON \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
      -DBUILD_WITH_OMP=0 -DBUILD_WITH_PERFETTO=OFF
cmake --build build-fuzz --target unitrace_fuzz
```

`BUILD_WITH_OMP=0` and `BUILD_WITH_PERFETTO=OFF`, as test_unitrace.py and
`coverage_report.sh` use: no fuzzed parser is in either, CMake's OpenMP lookup fails
for oneAPI's bare clang, and Perfetto would download and build protobuf.

Python: `pip install atheris` (plus the scripts' own requirements, `requirements.txt`).

## Run

```sh
cd tools/unitrace/test/fuzz
./run_fuzz.sh -t 3600 -s 1 -b ../../../../build-fuzz/test/fuzz/cpp          # all targets in parallel, 1 h each
./run_fuzz.sh -t 600 -b ../../../../build-fuzz/test/fuzz/cpp elf64 kmd_trace  # selected targets
```

Each target gets `results/campaign/<target>/` with the evolving `corpus/`,
reproducers in `artifacts/` (`crash-*`, `oom-*`, `timeout-*`), the full `fuzz.log`
and `run.json` (command, seed, duration, start/end, exit status). A target directory
that already exists (an earlier or concurrent run) is refused, so use a new `-o`
for each campaign; the script exits non-zero if any campaign failed or its
`run.json` could not be written. Limits: 10 s per
input, 1 GB per allocation, and an RSS limit of 2 GB (`-m`). Atheris' instrumentation
grows a Python fuzzing process by ~1.7 KB per input, so give hour-long Python runs
`-m 4096`.

The default `-b` is `../../build/test/fuzz/cpp` (a build in `tools/unitrace/build`);
for the `build-fuzz` build above, pass `-b` as shown.

Replay one input: `build-fuzz/test/fuzz/cpp/fuzz_<target> <file>` or `python3 python/fuzz_<target>.py <file>`.

## Coverage, regression, evidence

```sh
./coverage_report.sh results/campaign results/coverage   # llvm-cov and coverage.py, text/JSON/HTML
(cd .. && python3 test_unitrace.py --test-name 'fuzz_*') > results/regression.txt
python3 make_evidence.py --campaign main=results/campaign
```

`coverage_report.sh` needs `clang`, `llvm-profdata` and `llvm-cov` (or their `-18`
variants) and `coverage.py` (`pip install coverage`). It rebuilds unitrace with
coverage flags; extra CMake options for that build (e.g. `-DBUILD_WITH_XPTI=0`) go
in the `UNITRACE_CMAKE_ARGS` environment variable.

Each target is also a test in `test/tests.txt` (`[fuzz_<target>]`, running
`replay.py <target>`): `python test_unitrace.py` (in `test/`) builds the targets and
runs them with the other cpp tests, as CI does; `--test-name 'fuzz_*'` runs only
the fuzz tests, `--test-name fuzz_elf64` one target. A new target runs in CI once it
has its `[fuzz_<target>]` block there; a C++ target also needs its
`add_fuzz_target()` entry in `cpp/CMakeLists.txt`, or it is not built.

`regression/<target>/` holds every input that crashed a target; the fuzz tests
replay them with the seed corpus. Add a new crash input there when a finding is
fixed. `make_evidence.py` reads the register of findings and their disposition
from `findings.json` (not committed).
