# Unified Tracing and Profiling Tool

## Introduction

This a performance tool for Intel(R) oneAPI applications. It traces and profiles host/device activities, interactions and hardware utilizations for
Intel(R) GPU applications.

## Supported Platforms

- Linux
- Intel(R) oneAPI Base Toolkits
- Intel(R) GPUs including Intel(R) Data Center GPU Max Series

## Requirements

- CMake 3.22 or above (CMake versions prior to 3.22 are not fully tested or validated)
- C++ compiler with C++17 support
- Intel(R) oneAPI Base Toolkits
- Python 3.9 or later
- Matplotlib 3.8 or later (https://matplotlib.org/)
- Pandas 2.2.1 or later (https://pandas.pydata.org/)
- Intel(R) MPI (optional)

## Build and Install

```sh
set up Intel(R) oneAPI environment
set up Intel(R) MPI Environment (optional)
```

### Linux
```sh
cd <....>/tools/unitrace
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
or
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_MPI=<0|1> ..
or
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_MPI=<0|1> -DCMAKE_INSTALL_PREFIX=<installpath> ..
make
or
make install
```

### Windows
```sh
cd <....>\tools\unitrace
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
or
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_MPI=0 ..
or
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_MPI=0 -DCMAKE_INSTALL_PREFIX=<installpath> ..
nmake
or
nmake install
```

The MPI support is not enabled if **BUILD_WITH_MPI=0** is defined.

## Run

```sh
unitrace [options] <application> [args]
```

The options can be one or more of the following:

```
--call-logging [-c]            Trace host API calls
--host-timing  [-h]            Report host API execution time
--device-timing [-d]           Report kernels execution time
--ccl-summary-report [-r]      Report CCL execution time summary
--kernel-submission [-s]       Report append (queued), submit and execute intervals for kernels
--device-timeline [-t]         Report device timeline
--opencl                       Trace OpenCL
--chrome-mpi-logging           Trace MPI
--chrome-sycl-logging          Trace SYCL runtime and plugin
--chrome-ccl-logging           Trace oneCCL
--chrome-dnn-logging           Trace oneDNN
--chrome-itt-logging           Trace activities in applications instrumented using Intel(R) Instrumentation and Tracing Technology APIs
--chrome-call-logging          Trace Level Zero and/or OpenCL host calls
--chrome-kernel-logging        Trace device and host kernel activities
--chrome-device-logging        Trace device activities
--chrome-no-thread-on-device   Trace device activities without per-thread info.
                               Device activities are traced per thread if this option is not present
--chrome-no-engine-on-device   Trace device activities without per-Level-Zero-engine-or-OpenCL-queue info.
                               Device activities are traced per Level-Zero engine or OpenCL queue if this option is not present
--chrome-event-buffer-size <number-of-events>    Size of event buffer on host per host thread(default is -1 or unlimited)
--chrome-device-timeline       DEPRECATED - use --chrome-kernel-logging instead
--chrome-kernel-timeline       DEPRECATED - use --chrome-kernel-logging instead
--chrome-device-stages         DEPRECATED - use --chrome-kernel-logging instead
--verbose [-v]                 Enable verbose mode to show more kernel information.
--demangle                     Demangle kernel names. For OpenCL backend only. Kernel names are always demangled for Level Zero backend
--kernels-per-tile             DEPRECATED - use --separate-tiles instead
--separate-tiles               Trace each tile separately in case of implicit scaling
--tid                          Output TID in host API trace
--pid                          Output PID in host API and device activity trace
--output [-o] <filename>       Output profiling result to file
--conditional-collection       Enable conditional collection
--output-dir-path <path>       Output directory path for result files
--metric-query [-q]            Query hardware metrics for each kernel instance
--metric-sampling [-k]         Sample hardware performance metrics for each kernel instance in time-based mode
--group [-g] <metric-group>    Hardware metric group (ComputeBasic by default)
--sampling-interval [-i] <interval> Hardware performance metric sampling interval in us (default is 50 us) in time-based mode
--device-list                  Print available devices
--metric-list                  Print available metric groups and metrics
--stall-sampling               Sample hardware execution unit stalls. Valid for Intel(R) Data Center GPU Max Series and later GPUs
--ranks-to-sample <ranks>      MPI ranks to sample. The argument <ranks> is a list of comma separated MPI ranks
--version                      Print version
--help                         Show the help message and exit
```

## Level Zero or Level Zero + OpenCL

By default, only Level Zero tracing/profiling is enabled. To enable OpenCL tracing and profiling, please use --opencl option.

## View Event Trace

If one of more of the **--chrome-** options are present, a .json trace file will be generated. To view the event trace, one can load
the .json trace file to **https://ui.perfetto.dev/** in either Google Chrome or Microsoft Edge browser.

![Host-Device Timelines!](/tools/unitrace/doc/images/host-device-times.png)

Do **NOT** use **chrome://tracing/** to view the event trace!

## Host Level Zero and/or OpenCL Activities

To trace/profile Level Zero and/or OpenCL host activities, one can use one or more of the following options:

--call-logging [-c]
--host-timing  [-h]
--chrome-call-logging

The **--call-logging [-c]** option traces Level Zero and/or OpenCL calls on the host:
![Host Call Logging!](/tools/unitrace/doc/images/call-trace.png)

The **--host-timing  [-h]** option outputs a Level Zero and/or OpenCL host call timing summary:
![Host Call Timing!](/tools/unitrace/doc/images/host-timing.png)

The **--chrome-call-logging** option generates a Level Zero and/or OpenCL host .json event trace that can be viewd in **https://ui.perfetto.dev/**:
![Host Event Trace!](/tools/unitrace/doc/images/call-logging.png)


## Device and Kernel Activities

To trace/profile device and kernel activities, one can use one or more of the following options:

--device-timing [-d]
--kernel-submission [-s]
--device-timeline [-t]
--chrome-kernel-logging
--chrome-device-logging
--chrome-no-thread-on-device
--chrome-no-engine-on-device

The **--device-timing [-d]** option outputs a timing summary of kernels and commands executed on the device:


![Device Timing With No Shape!](/tools/unitrace/doc/images/device-timing-with-no-shape.png)

In addition, it also outputs kernel information that helps to identify kernel performance issues that relate to occupancy caused by shared local memory usage and register spilling.

![Kernel Info With No Shape!](/tools/unitrace/doc/images/kernel-info-with-no-shape.png)

Here, the **"SLM Per Work Group"** shows the amount of shared local memory needed for each work group in bytes. This size can potentially affect occupancy.

The **"Private Memory Per Thread"** is the private memory allocated for each thread in bytes. A non-zero value indicates that one or more thread private variables are not in registers.

The **"Spill Memory Per Thread"** is the memory used for register spilled for each thread in bytes. A non-zero value indicates that one or more thread private variables are allocated in registers but are later spilled to memory.

The **"Register File Size Per Thread"** is the number of registers available to each thread. In case the Level Zero runtime is not up-to-date, this column may be shown as **"unknown"**.

In addition, the **"Compiled"** indicates the kernel compilation mode: just-in-time(JIT) or ahead-of-time(AOT).

By default, the kernel timing is summarized regardless of shapes. In case the kernel has different shapes, using **-v** along with **-d** is strongly recommended:

![Device Timing!](/tools/unitrace/doc/images/device-timing.png)

![Kernel Info!](/tools/unitrace/doc/images/kernel-info.png)


The **--kernel-submission [-s]** option outputs a time summary of kernels spent in queuing, submission and execution:
![Kernel Submissions!](/tools/unitrace/doc/images/kernel-submissions.png)

The **--device-timeline [-t]** option outputs timestamps of each kernel instance queuing, submission and execution start and execution end in text format while the application is running.

![Device Timeline Text!](/tools/unitrace/doc/images/device-timeline-text.png)

Both **--chrome-kernel-logging** and **--chrome-device-logging** create device event trace and store it in a .json file. However, **--chrome-kernel-logging** also traces host-device interactions and dependencies in addition to device events.

Kernel Loggging:

![Kernel Logging!](/tools/unitrace/doc/images/kernel-logging.png)

Device Logging:

![Device Logging!](/tools/unitrace/doc/images/device-logging.png)

In case both **--chrome-kernel-logging** and **--chrome-device-logging** are present, **--chrome-kernel-logging** takes precedence.

### Toggling Device Thread, Level-Zero Engine and OpenCL Queue Collection On/Off

By default, device activities are profiled per thread, per Level-Zero engine and per OpenCL queue (if OpenCL profiling is enabled):

![Device Activities Per Thread and Per Level-Zero Engine and Per OpenCL Queue!](/tools/unitrace/doc/images/device-per-thread-per-engine.png)

This can potentially take a lot of screen space if a lot of threads use the device and/or multiple Level-Zero engines and/or OpenCL queues are utilized when the trace is viewed in a browser.

You can use **--chrome-no-thread-on-device** to suppress thread data:

![Device Activities Per Level-Zero Engine and Per OpenCL Queue!](/tools/unitrace/doc/images/device-per-engine.png)

or **--chrome-no-engine-on-device** to suppress engine and/or queue data:

![Device Activities Per Thread!](/tools/unitrace/doc/images/device-per-thread.png)

or both to suppress thread and engine and/or queue data at the same time:

![Device Activities!](/tools/unitrace/doc/images/device-no-thread-no-engine.png)


### Tile Activities in Implicit Scaling

In case of implicit scaling, the hardware partitions the load of each kernel and distributes portions to each tile. By default, the tool treats the whole device as a unit. To trace activities on each tile separately, one can use **--separate-tiles** together with **--chrome-kernel-logging/--chrome-device-logging** options:

![Tile Activities Logging!](/tools/unitrace/doc/images/implicit-per-tile-kernel-logging.png)

or with **--device-timing [-d]** option:

![Tile Activities Timing!](/tools/unitrace/doc/images/implicit-per-tile-timing.png)

## Trace and Profile Layers above Level Zero/OpenCL

The **--chrome-mpi-logging** traces MPI activities

![MPI Logging!](/tools/unitrace/doc/images/mpi-logging.png)

The **--chrome-sycl-logging** traces SYCL runtime and SYCL Plugins activities

![SYCL Logging!](/tools/unitrace/doc/images/sycl-logging.png)

The **--chrome-ccl-logging** traces CCL runtime activities
![CCL Logging!](/tools/unitrace/doc/images/ccl_logging.png)

Similarly, one can use **--chrome-dnn-logging** for oneDNN.

The **--chrome-itt-logging** traces activities in applications instrumented using Intel(R) Instrumentation and Tracing Technology APIs

![ITT logging!](/tools/unitrace/doc/images/chrome_itt_logging.png)

The **--ccl-summary-report  [-r]** option outputs CCL call timing summary:
![CCL Call Timing!](/tools/unitrace/doc/images/ccl_summary_report.png)

If the application is a PyTorch workload, presence of **--chrome-mpi-logging** or **--chrome-ccl-logging** or **--chrome-dnn-logging** also enables PyTorch profiling(see **Profile PyTorch** section for more information).

## Location of Trace Data

By default, all output data are written in the current working directory. However, one can specify a different directory for output:

```
--output-dir-path <path>
```

This option is especially useful when the application is a distributed MPI one.

## Activate and Deactivate Tracing and Profiling at Runtime

By default, the application is traced/profiled from the start to the end. In certain cases, however, it is more efficient and desirable to
dynamically activate and deactivate tracing at runtime. You can do so by using **--conditional-collection** option together with setting and
unsetting environment variable **"PTI_ENABLE_COLLECTION"** in the application:

```cpp
// activate tracing
setenv("PTI_ENABLE_COLLECTION", "1", 1);
// tracing is now activated

......

// deactivate tracing
setenv("PTI_ENABLE_COLLECTION", "0", 1);
// tracing is now deactivated
```

The following command traces events only in the code section between **setenv("PTI_ENABLE_COLLECTION", "1", 1)** and
**unsetenv("PTI_ENABLE_COLLECTION")**.

```sh
unitrace --chrome-call-logging --chrome-kernel-logging --conditional-collection <application> [args]
```

You can also use __itt_pause() and __itt_resume() APIs to pause and resume collection:

```cpp
__itt_pause();
// tracing is now deactivated

......

__itt_resume();
// tracing is now activated
```

If both environment variable **PTI_ENABLE_COLLECTION** and **__itt_pause()/__itt_resume()** are present, **__itt_pause()/__itt_resume()** takes precedence.

By default, collection is disabled when the application is started. To change the default, you can start the application with  **PTI_ENABLE_COLLECTION** set to 1, for example:

```sh
PTI_ENABLE_COLLECTION=1 unitrace --chrome-call-logging --chrome-kernel-logging --conditional-collection <application> [args]
```

If **--conditional-collection** option is not specified, however, PTI_ENABLE_COLLECTION settings or __itt_pause()/__itt_resume() calls have **no** effect and the application is traced/profiled from the start to the end.

## Profile MPI Workloads

### Run Profiling

To profile an MPI workload, you need to launch the tool on each rank, for example:

```sh
mpiexec <options> unitrace ...
```

You can also do selective rank profiling by launching the tool only on the ranks of interest.

The result .json file has the rank id embedded as **<application>.<pid>.<rank>.json**.


### View Event Timelines of Multiple MPI Ranks

You can view the result files rank by rank. But often you may want to view the traces from multiple ranks at the same time.
To view traces from multiple MPI ranks, you can use **scripts/tracemerge/mergetrace.py** script to merge them first and then load the merged trace into
https://ui.perfetto.dev/.

```sh
python mergetrace.py -o <output-trace-file> <input-trace-file-1> <input-trace-file-2> <input-trace-file-3> ...
```
![Multiple MPI Ranks Host-Device Timelines!](/tools/unitrace/doc/images/multipl-ranks-timelines.png)

## Profile PyTorch

To profile PyTorch, you need to enclose the code to be profiled with

```sh
with torch.autograd.profiler.emit_itt():
    ......
```

For example:

```sh
with torch.autograd.profiler.emit_itt(record_shapes=False):
    for batch_idx, (data, target) in enumerate(train_loader):
        optimizer.zero_grad()
        data = data.to("xpu")
        target = target.to("xpu")
        with torch.xpu.amp.autocast(enabled=True, dtype=torch.bfloat16):
            output = model(data)
            loss = criterion(output, target)
        loss.backward()
        optimizer.step()
```

To profile PyTorch, option **--chrome-mpi-logging** or **--chrome-ccl-logging** or **--chrome-dnn-logging** must be present. For example:

```sh
unitrace --chrome-kernel-logging --chrome-dnn-logging --chrome-ccl-logging python ./rn50.py
```

![PyTorch Profiling!](/tools/unitrace/doc/images/pytorch.png)

You can use **PTI_ENABLE_COLLECTION** environment variable to selectively enable/disable profiling.

```sh
with torch.autograd.profiler.emit_itt(record_shapes=False):
    os.environ["PTI_ENABLE_COLLECTION"] = "1"
    for batch_idx, (data, target) in enumerate(train_loader):
        optimizer.zero_grad()
        data = data.to("xpu")
        target = target.to("xpu")
        with torch.xpu.amp.autocast(enabled=True, dtype=torch.bfloat16):
            output = model(data)
            loss = criterion(output, target)
        loss.backward()
        optimizer.step()
        if (batch_idx == 2):
            os.environ["PTI_ENABLE_COLLECTION"] = "0"
```
Alternatively, you can use itt-python to do selective profiling as well. The itt-python can be installed from conda-forge

```sh
conda install -c conda-forge --override-channels itt-python
```

```sh
import itt
......

with torch.autograd.profiler.emit_itt(record_shapes=False):
    itt.resume()
    for batch_idx, (data, target) in enumerate(train_loader):
        optimizer.zero_grad()
        data = data.to("xpu")
        target = target.to("xpu")
        with torch.xpu.amp.autocast(enabled=True, dtype=torch.bfloat16):
            output = model(data)
            loss = criterion(output, target)
        loss.backward()
        optimizer.step()
        if (batch_idx == 2):
            itt.pause()
```

```sh
unitrace --chrome-kernel-logging --chrome-dnn-logging --conditional-collection python ./rn50.py
```

## View Large Traces

By default, the memory limit of the internal representation of a trace is 2GB. To view large traces that requires more than 2GB of memory, an external trace processor is needed.

One way to run the external trace processor on Windows is to use Windows Subsystem for Linux or WSL:

1. Start your Linux distribution (Ubuntu 22.02.2 LTS, for example) on Windows.
2. Inside Linux, download and run trace_processor:
  
   ```sh
   curl -LO https://get.perfetto.dev/trace_processor
   chmod +x ./trace_processor
   trace_processor --httpd
   ```
3. Start the browser in Windows, load https://ui.perfetto.dev/ and open the trace file. If the trace is loaded successfully, inside Linux, you
   should see something like:


   ```sh
   $ ./trace_processor --httpd
   [924.614]             httpd.cc:99 [HTTP] Starting RPC server on localhost:9001
   [924.614]            httpd.cc:104 [HTTP] This server can be used by reloading https://ui.perfetto.dev and clicking on YES on the "Trace Process   or native acceleration" dialog or through the Python API (see https://perfetto.dev/docs/analysis/trace-processor#python-api).
   [957.821]       http_server.cc:83 [HTTP] New connection
   [957.822]      http_server.cc:231 [HTTP] POST /status [body=0B, origin="https://ui.perfetto.dev"]
   [957.892]       http_server.cc:83 [HTTP] New connection
   [957.894]      http_server.cc:231 [HTTP] GET /websocket [body=0B, origin="https://ui.perfetto.dev"]
   Loading trace 2004.29 MB (0.7 MB/s)
   ```
## Profile Hardware Performance Metrics

Hardware performance metric counter can be profiled at the same time while host/device activities are profiled in the same run or they can be done in separate runs.

Please note that device timing is also enabled if hardware performance metric counter profiling is enabled. The device timing information will guide you to the hot kernels so you know which kernel's performance counters are of most interest.

Please also note that FLAT device hierarchy is required for hardware metric profiling.

### Query Metrics for Each Kernel Instance

The **--metric-query [-q]** option enables metric query for each kernel instance.

   ```sh
   unitrace -q -o perfquery.csv myapp
   ```
Performance metrics data will be stored in **perfquery.<pid>.csv** file.

![Metric Query!](/tools/unitrace/doc/images/metric-query.png)

By default, counters in **ComputeBasic** metric group are profiled. You can use the **--group [-g]** option to specify a different group. All available metric groups can be listed by **--metric-list** option.

### Sample Metrics in Time-based Mode

Different from **--metric-query [-q]** option, the **--metric-sampling [-k]** option profile hardware metrics in time-based sampling mode.

   ```sh
   unitrace -k -o perfmetrics.csv myapp
   ```
Performance metrics data will be stored in **perfmetrics.<pid>.csv** file.

![Metric Sampling!](/tools/unitrace/doc/images/metric-sampling.png)

To kernels that take short time, you may find that the default sampling rate is not high enough and the sampling rate or the sampling interval needs to be adjusted using **--sampling-interval [-i]** option, for example:


   ```sh
   unitrace -k -i 20 -o perfmetrics.csv myapp
   ```

By default, counters in **ComputeBasic** metric group are profiled. You can use the **--group [-g]** option to specify a different group. All available metric groups can be listed by **--metric-list** option.

### Sample Stalls at Instruction Level

The **--stall-sampling** works on Intel(R) Data Center GPU Max Series and later products.

![Metric Query!](/tools/unitrace/doc/images/stall-sampling.png)

To kernels that take short time, you may find that the default sampling rate is not high enough and the sampling rate or the sampling interval needs to be adjusted using **--sampling-interval [-i]** option.

### Sample Metrics of MPI Ranks

If the workload is an MPI application, sampling multiple ranks running on the same node with **-k** or **--stall-sampling** is usually unnecessary. You can use option **--ranks-to-sample** to specify which rank/ranks to sample. If the workload has 8 ranks running on 2 nodes with 4 ranks each, for example, you can sample rank #0 on one node and rank #4 on the other node:

   ```sh
   mpiexec -n 8 -ppn 4 unitrace -k -o perfmetrics.csv --ranks-to-sample 0,4 <app>
   ```

### Analyze Performance Metrics

Once you have the hardware performance metrics data collected, you can use the script **scripts/metrics/analyzeperfmetrics.py** to analyze the metrics. 

#### List Contents of the Metric Data File

The first step is to inspect the contents of the metric data file using **-l** or **--list** option. 

   ```sh
   python analyzeperfmetrics.py -l perfmetrics.12345.csv
   ```

This shows the device, metrics and kernels profiled:

```sh
Device 0
    Metric
        GpuTime[ns]
        GpuCoreClocks[cycles]
        AvgGpuCoreFrequencyMHz[MHz]
        GpuSliceClocksCount[events]
        AvgGpuSliceFrequencyMHz[MHz]
        L3_BYTE_READ[bytes]
        L3_BYTE_WRITE[bytes]
        GPU_MEMORY_BYTE_READ[bytes]
        GPU_MEMORY_BYTE_WRITE[bytes]
        XVE_ACTIVE[%]
        XVE_STALL[%]
        XVE_BUSY[events]
        XVE_THREADS_OCCUPANCY_ALL[%]
        XVE_COMPUTE_THREAD_COUNT[threads]
        XVE_ATOMIC_ACCESS_COUNT[messages]
        XVE_BARRIER_MESSAGE_COUNT[messages]
        XVE_INST_EXECUTED_ALU0_ALL[events]
        XVE_INST_EXECUTED_ALU1_ALL[events]
        XVE_INST_EXECUTED_XMX_ALL[events]
        XVE_INST_EXECUTED_SEND_ALL[events]
        XVE_INST_EXECUTED_CONTROL_ALL[events]
        XVE_PIPE_ALU0_AND_ALU1_ACTIVE[%]
        XVE_PIPE_ALU0_AND_XMX_ACTIVE[%]
        XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%]
        XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%]
        XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%]
        XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%]
        XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]
        QueryBeginTime[ns]
        CoreFrequencyMHz[MHz]
        XveSliceFrequencyMHz[MHz]
        ReportReason
        ContextIdValid
        ContextId
        SourceId
        StreamMarker
    Kernel, Number of Instances
        "main::{lambda(auto:1)#1}[SIMD32 {8192; 1; 1} {128; 1; 1}]", 1
        "main::{lambda(auto:1)#2}[SIMD32 {8192; 1; 1} {128; 1; 1}]", 1
        "main::{lambda(auto:1)#3}[SIMD32 {8192; 1; 1} {128; 1; 1}]", 1
        "main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]", 5
        "main::{lambda(auto:1)#5}[SIMD32 {4096; 1; 1} {256; 1; 1}]", 5
        "main::{lambda(auto:1)#6}[SIMD32 {4096; 1; 1} {256; 1; 1}]", 5
        "main::{lambda(auto:1)#7}[SIMD32 {2048; 1; 1} {512; 1; 1}]", 5
```

The **Device** is the device on which the metrics are sampled. In this example output, the decice is 0. If multiple devices are used and sampled, multiple sections of **Device** will be present.

The **Metric** section shows the metrics collected on the device and the **Kernel, Number of Instances** shows the kernels and number of instances for each kernel are profiled. An instance is one kernel execution sampled on the device. For example, The kernel "main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]" having 5 instances means the 5 exeuctions of the kernel are sampled. Please note that the number of instances of a kernel here may be less than the total number of exeuctions or submissions of the kernel in the application, especially when the kernel is short and/or sampling interval is large. 

The number of instances is not applicable to stall sampling metric data:

```sh
Device 0
    Metric
        Active[Events]
        ControlStall[Events]
        PipeStall[Events]
        SendStall[Events]
        DistStall[Events]
        SbidStall[Events]
        SyncStall[Events]
        InstrFetchStall[Events]
        OtherStall[Events]
    Kernel
        "main::{lambda(auto:1)#1}"
        "main::{lambda(auto:1)#2}"
        "main::{lambda(auto:1)#3}"
        "main::{lambda(auto:1)#5}"
        "main::{lambda(auto:1)#6}"
        "main::{lambda(auto:1)#7}"
        "main::{lambda(auto:1)#4}"
```

You can also use the **-o** option to redirect the output to a text file for later reference:

   ```sh
   python analyzeperfmetrics.py -l -o contents.txt perfmetrics.12345.csv
   ```


#### Analyze Kernel Performance Metrics

Once you have the knowledge of the device, the metrics and the kernels in the metric data file, you can run the same script to analyze specific performance metrics of a specific instance of a specific kernel, all instances of a specific kernel or all instances of all kernels executed on a specific device. The performance chart will be stored in a PDF file. 

   ```sh
   python analyzeperfmetrics.py -d 0 -k "main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]" -i 2 -m "XVE_STALL[%],XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%],XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%],XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%],XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%],XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]" -y "Utilization and Stall (%)" -t "Utilization and Stall" -o perfchart.pdf perfmetrics.12345.csv
   ```

This command plots a chart of XVE stall and function unit utilizations for the **second** instance of kernel **"main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]"** profiled on device **0** and stores the chart in file **perfchart.pdf**.

![Analyze Kernel Performance Metrics!](/tools/unitrace/doc/images/perfchart.png)

If instance is 0, all 5 instances of the kernel **"main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]"** are analyzed.

   ```sh
   python analyzeperfmetrics.py -d 0 -k "main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]" -i 0 -m "XVE_STALL[%],XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%],XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%],XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%],XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%],XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]" -y "Utilization and Stall (%)" -t "Utilization and Stall" -o perfchart.pdf perfmetrics.12345.csv

   ```

If **-k** option is not present and instance is 0, all instances of all kernels are analyzed.

   ```sh
   python analyzeperfmetrics.py -d 0 -i 0 -m "XVE_STALL[%],XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%],XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%],XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%],XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%],XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]" -y "Utilization and Stall (%)" -t "Utilization and Stall" -o perfchart.pdf perfmetrics.12345.csv

   ```

The **-m** option can be repeated multiple times to analyze multiple sets of metrics at the same time, for example:

   ```sh
   python analyzeperfmetrics.py -d 0 -k "main::{lambda(auto:1)#4}[SIMD32 {4096; 1; 1} {256; 1; 1}]" -i 2 -m "XVE_STALL[%],XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%],XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%],XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%],XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%],XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]" -y "Utilization and Stall (%)" -m "L3_BYTE_READ[bytes],L3_BYTE_WRITE[bytes]" -y "L3 Cache Read/Write (bytes)" -o perfchart.pdf perfmetrics.12345.csv
   ```

![Analyze Multiple Performance Metric Sets!](/tools/unitrace/doc/images/perfchart-multi-sets.png)

Instead of typing the command options in every run, you can store the options in a text configuration file and use the **-f** or **--config** option to read the options from the file. For example, the command options in the above can be stored in a **myconfig.txt**:

   ```sh
   python analyzeperfmetrics.py -f myconfig.txt -o perfchart.pdf perfmetrics.12345.csv
   ```

Please note that input file cannot be in the command option configuration file.
You can also use the **-b** option together with one or more **-m** options to get bandwidthi data.

![Analyze Multiple Performance Metric Sets and Bandwidths!](/tools/unitrace/doc/images/bandwidth.png)

If the input metric data file has stall sampling events collected using **--stall-sampling** option, the chart generated shows stall events and instruction addresses.

![Analyze Stall Metrics!](/tools/unitrace/doc/images/stallchart.png)

From this chart, we can easily see that the most stalls are **SbidStalls** at instruction **0x000001B8**. To reduce or eliminate the stalls, we need to analyze the stalls at instruction level to find out the cause of the stalls.

##### Use Pre-configured Options #####

A pre-configured option file **metrics/config/ComputeBasic.txt** for **ComputeBasic** (the default metric group) on PVC is provided. You can use it as it is:

   ```sh
   python analyzeperfmetrics.py -f config/pvc/ComputeBasic.txt -o perfchart.pdf perfmetrics.12345.csv
   ```

Or you can customize it to create your own recipes. 

You may also want to create configurations for other metric groups and/or devices.

#### Analyze Stalls at Instruction Level

To pinpoint stalls to exact instructions, you need kernel shader dump. If you want to pinpoint to the source lines and source files, you also need to use the compiler option **-gline-tables-only**, for example:

    ```sh
    icpx -fsycl -gline-tables-only -O2 -o mytest mytest.cpp
    ````

To get a kernel shader dump, run the application with environment variables **IGC_ShaderDumpEnable** and **IGC_DumpToCustomDir** set:

    ```sh
    IGC_ShaderDumpEnable=1 IGC_DumpToCustomDir=dump mytest
    ````

After you run unitrace using **--stall-sampling**, you can run the same script to analyze stalls:

    ```sh
     python analyzeperfmetrics.py -k "main::{lambda(auto:1)#3}" -s ./dump -o stallchart.pdf ./stallmetrics.56789.csv
    ```

In addition to the stall statistics chart, a stall analysis report is also generated:

```sh
Kernel: main::{lambda(auto:1)#3}
Assembly with instruction addresses: ./dump.3/OCL_asme5a3f8d8dcdadd7e_simd32_entry_0003.asm.ip
***********************************************************************************************
Sbid Stalls:

Instruction
  /* [000001B8] */         sync.nop                             null                             {Compacted,$5.dst}     // $15
  Line 40:  c[index] = a[index] + b[index];
  File: /nfs/pdx/home/zma2/Box/PVC/grf.cpp
is stalled potentially by
  instruction
    /* [00000198] */         load.ugm.d32.a64 (32|M0)  r18:2         [r14:4]            {I@1,$5} // ex_desc:0x0; desc:0x8200580 // $14
    Line 40:  c[index] = a[index] + b[index];
    File: /nfs/pdx/home/zma2/Box/PVC/grf.cpp

Instruction
  /* [00000118] */ (W)     mul (1|M0)               acc0.0<1>:d   r5.0<0;1,0>:d     r0.2<0;1,0>:uw   {Compacted,A@1,$3.dst} //  ALU pipe: int; $4
  Line 96:  return __spirv_BuiltInGlobalInvocationId.x;
  File: /opt/hpc_software/compilers/intel/nightly/20240527/compiler/latest/bin/compiler/../../include/sycl/CL/__spirv/spirv_vars.hpp
is stalled potentially by
  instruction
    /* [00000100] */ (W)     load.ugm.d32x8t.a32.ca.ca (1|M0)  r5:1  bti[255][r126:1]   {I@1,$3} // ex_desc:0xFF000000; desc:0x6218C500 //
    Line 1652:  }
    File: /opt/hpc_software/compilers/intel/nightly/20240527/compiler/latest/bin/compiler/../../include/sycl/handler.hpp
```

The report shows not only the instruction and the location (address, source line and source file if available) of each stall, but also the instruction and the location if available that causes the stall.

To eliminate or reduce the stalls, you need to fix the cause.

The stall analysis report can also be stored in a text file if you prefer:


    ```sh
     python analyzeperfmetrics.py -k "main::{lambda(auto:1)#3}" -s ./dump -o stallchart.pdf -r stallreport.txt ./stallmetrics.56789.csv
    ```

## View Kernel Performance Metrics in Browser

If you use **-k** or **--stall-sampling** option together with **--chrome-kernel-logging** or **--chrome-device-logging** option, you can view performance metrics of each kernel instance by selecting the kernel instance then following the metrics link in the "Arguments" in the browser.

    
    ```sh
     $ unitrace -k --chrome-kernel-logging -o perf.csv ./testapp
     ... ...

     [INFO] Log is stored in perf.1092793.csv
     [INFO] Timeline is stored in testapp.1092793.json
     [INFO] Device metrics are stored in perf.metrics.1092768.csv
    ```

The performance metrics are stored in file **perf.metrics.1092768.csv** and the event trace is stored in the .json file. 

Run **analyzeperfmetrics.py** in a shell window with **-p** option, for example:

    ```sh
     $ python analyzeperfmetrics.py -p -m "XVE_STALL[%],XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION[%],XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION[%],XVE_INST_EXECUTED_SEND_ALL_UTILIZATION[%],
     XVE_INST_EXECUTED_CONTROL_ALL_UTILIZATION[%],XVE_INST_EXECUTED_XMX_ALL_UTILIZATION[%]" -y "Stall and Utilizations" -t "Stall and Utilizations" ./perf.metrics.1092768.csv
    ```

The **-p** option starts a server. If no certificate and private key are provided, a self-signed certificate and private key will be generated and used.
    

Now load the event trace .json file into https://ui.perfetto.dev:


![Performance Metrics Through Event Trace!](/tools/unitrace/doc/images/perfmetricstrace.png)
    

Once you click the link next to **metrics:** in the **"Arguments"**, another browser window is opened:


![Performance Metrics Browswe Window!](/tools/unitrace/doc/images/perfmetricsbrowser.png)


The metrics shown in the browser are the metrics passed to the **-m** option when you start **analyzeperfmetrics.py**. If you stop and restart **analyzeperfmetrics.py** with a different set of metrics passed to **-m** option, for example:

    ```sh
     $ python analyzeperfmetrics.py -p -m "L3_BYTE_READ[bytes],L3_BYTE_WRITE[bytes]" -y "Bytes" -t "L3 Traffic" ./grfbasic.metrics.1092768.csv
    ```

Refreshing the same link will show the new metrics:

![Performance Metrics Browswe Window #2!](/tools/unitrace/doc/images/perfmetricsbrowser2.png)

In case of stall sampling, for example:

    ```sh
     $ unitrace --stall-sampling --chrome-kernel-logging -o perfstall.csv ./testapp
    ```

No **-m** option is required for **analyzeperfmetrics.py**:

    ```sh
    python analyzeperfmetrics.py -s ./dump.1 -p ./perfstall.metrics.564289.csv -t "XVE Stall Statistics and Report"
    ```

Rereshing the same link will show stall statistics by type and instruction address:

![Stall Statistics!](/tools/unitrace/doc/images/stallstatistics.png)

followed by source stall analysis report:

![Stall Report!](/tools/unitrace/doc/images/stallreport.png)
 
## Query Trace Events

Search in https://ui.perfetto.dev/ can be done in command or SQL. After loading the trace file user can switch to SQL mode by typing ":" in the search box, then the user can type in the SQL query statement(s) and navigate to the events of interest from the query result, as shown below.

Please refer to https://perfetto.dev/docs/ for more information,

![Query Trace Events!](/tools/unitrace/doc/images/event_query.png)

## Recommended Usage

How to make the best use of the tool and to get the most out of it really depends on what you want to do and what part of your code you want to focus on.

If you care about just the host activities, you don't need any options to enable profiling on the device. If you just want to focus on one specific layer of the software stack, for example, the SYCL runtime and SYCL Plugins, you can use the corresponding layer specific options to enable profiling only the layer of interest, for example, **--chrome-sycl-logging**. Of course, if you need to see interactions between layers, you need to enable profiling multiple layers at the same time.

Similarly, if you care about just the device activities, you can use the options to turn on device profiling only. By default, device activities are profiled by thread (not GPU thread) and by Level Zero engines and/or OpenCL queues. This gives detailed information of how the device is utilized and if concurrencies between engines/queues match the expectations. In case you don't need the details and care only how the device is utilized in general, you may use **--chrome-no-thread-on-device** and/or **--chrome-no-engine-on-device** to turn one or both off.

Typically, you need options to enable profiling both host activities and device activities to understand how host and device are utilized and interacted. Ideally, you want both the concurrencies or overlappings between host activities and device activities and the concurrencies or overlappings between device engines maximized for best performance. Turning on profiling both host and device will give you the timelines of both and help you identify concurrency issues.

It is also recommended to start with device timing (**-d**) and/or host timing (**-h**) summaries. From the summaries, you can quickly spot the hot spots or the expensive kernels on device or calls on host. From the device summary, you will also learn if each kernel has occupancy and/or register spilling issues. Next, from the detailed timelines you will determine if these expensive device kernels or host calls are indeed performance bottlenecks to the overall performance.

Once a kernel is determined to be a performance bottleneck, it is time to figure out why its performance is not optimal. There can be multiple reasons why the kernel is not performant: cache misses, low occupancy, low ALU utilizations, execution unit stalls, etc. You can get answers from metric profiles using **-q** option. In case of execution unit stall analysis, the **--stall-sampling** will give you instruction addresses and reasons of stalls.

In an interactive, for example Python, session, the **-t** option can be very useful with the kernel queuing, submission and execution data are output immediately after each kernel completes while your session is active.
