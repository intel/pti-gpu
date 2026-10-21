# PC Sampling Profiler Sample

This sample collects PC Sampling (EuStallSampling) data from an **unmodified** application (Level Zero, SYCL, or PyTorch XPU).
The launcher `pc_sampling_profiler` preloads the collector library
`libpc_sampling_profiler_collector.so` into the application; the collector drives the PTI PC
Sampling API and prints a per-kernel, per-instruction stall report.

The PTI calls and the report come from the client in the [`pc_sampling`](../pc_sampling) sample;
this sample only decides when collection starts and stops.

## Requirements

- Linux and Intel oneAPI (`icpx`).
- Python with PyTorch XPU support, compatible with the sourced oneAPI, for the PyTorch workload.
- A GPU and driver with PC Sampling support.

## Build

The sample is built with the PTI SDK, which also provides Level Zero. From the `sdk` directory,
with the oneAPI environment set up, into a new build directory:

```bash
cmake -B build-pc-sampling -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/icpx_toolchain.cmake
cmake --build build-pc-sampling --parallel --target pc_sampling_profiler \
  pc_sampling_profiler_collector pc_sampling_profiler_l0_gemm pc_sampling_profiler_gemm
```

The launcher embeds the absolute path of the collector library, so the build directory cannot be
moved or installed.

## Run

From the `sdk` directory, in the same terminal, writing each report to `reports/`:

```bash
B=build-pc-sampling
mkdir -p reports
$B/bin/pc_sampling_profiler -o reports/l0.txt -- \
  $B/bin/pc_sampling_profiler_l0_gemm $B/samples/pc_sampling_profiler/gemm.spv
$B/bin/pc_sampling_profiler -o reports/sycl.txt -- \
  $B/bin/pc_sampling_profiler_gemm --size 2048 --repeat 3
$B/bin/pc_sampling_profiler -o reports/pytorch.txt -- \
  python samples/pc_sampling_profiler/pytorch_train.py
```

The bundled workloads are a Level Zero GEMM (`gemm.spv` is built from `gemm.cl`), a SYCL GEMM,
and a small PyTorch XPU training script (`pytorch_train.py`). None of them uses PTI. Any Level
Zero, SYCL, or PyTorch XPU application can be profiled the same way.

| Option | Description |
| --- | --- |
| `--period`, `-p` `<ns>` | Sampling period, 1000-10000000 ns (default 50000) |
| `--output`, `-o` `<file>` | Write the report to `<file>` instead of standard error |
| `--help`, `-h` | Usage |

The launcher `exec`s the application, so its exit status is preserved.

## How it works

`LD_PRELOAD` is what lets the collector run inside an unmodified application. The collector
interposes a single function, glibc's `__libc_start_main`, to wrap the application's `main`:

1. Before `main`, it enables the PTI kernel view and PC sampling and starts collection. PTI
   initializes Level Zero and enables metrics itself, so
   every module and kernel the application creates afterwards is observed, whichever runtime
   (Level Zero, SYCL, PyTorch) creates it.
2. After `main` returns, while the GPU runtime is still alive, it stops collection and writes the
   report: stall reasons, sample totals per device, and aggregated and per-instruction stall
   counts for every sampled kernel. The report goes to standard error unless `--output` is
   given; errors always go to standard error.

No Level Zero or higher-level runtime API is interposed, so the collector works the same with any
Level Zero loader.

`LD_PRELOAD` is inherited by child processes, but a process that enables PC sampling can block
while another one is sampling the GPU. The collector therefore samples only the first process
that loads it: that process records its PID in `PTI_PC_SAMPLING_PID`, and descendants that see a
different PID run unprofiled. The launcher clears the variable before `exec`, and the application
keeps the launcher's PID.

## Known limitations

- Only the launched process is profiled.
- Static and setuid binaries cannot be profiled: the dynamic loader ignores `LD_PRELOAD` for them.
- The target application must not use the PTI SDK itself. Only one PC sampling handle can exist.
- Threads of the profiled process share one collection. Children created with `fork()` without
  `exec()` inherit the parent's collector and GPU runtime state and are not supported.
- Other GPU processes running at the same time contend for the EU stall stream.
- No report is produced when the application does not return from `main`: calling `exit()`
  (including `std::exit`) or `_exit()`, being interrupted with Ctrl-C, or being killed by a
  signal.
- The application's exit status does not reflect profiling failures; check standard error.
