==============
What's New
==============

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Version 1.2.0
---------------

*Current release*

**Updates in this release:**

* Add the new ``ptiMetricsEnable`` and ``ptiMetricsDisable`` APIs, which enable metrics collection at runtime and remove the need to set the ``ZET_ENABLE_METRICS=1`` environment variable before the process starts. Calls are reference counted per device and must be matched. ``ptiMetricsEnable`` returns the new ``PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED`` result code when the installed driver does not support runtime enabling, in which case the environment variable is still required; other driver failures of both APIs are reported as ``PTI_ERROR_DRIVER``. The new ``PTI_ERROR_METRICS_ENABLE_FAILED`` result code is also added, for metrics that could not be enabled on all devices.
* Make ``ptiPcSamplingConfigure`` optional. If it is not called, ``ptiPcSamplingStartCollection`` applies the default configuration (the first device that supports PC sampling, sampling period of 100000 ns). Configuration failures are now reported with the new ``PTI_ERROR_PC_SAMPLING_CONFIGURATION_FAIL`` result code; ``PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED`` is kept for backward compatibility.
* **Behavior change:** ``libpti_view.so`` now registers itself as the ITT collector at load time, so oneCCL communication tracing works without extra environment setup. When ``INTEL_LIBITTNOTIFY64`` is unset, a static initializer sets it to the absolute path of ``libpti_view.so`` before ``main()``. As a result, ITT is loaded in every process that loads ``libpti_view.so``, even if ``PTI_VIEW_COMMUNICATION`` is never enabled, and child processes (for example ``torchrun`` or ``mpirun`` workers) inherit the variable. To opt out, set the variable to another collector before the process starts; PTI then leaves it unchanged and disables its own oneCCL tracing. See :doc:`knownissues`.
* Significantly reduce tracing overhead for Level Zero native graph execution (about 40% lower in internal measurements) by gathering the timestamps of the whole graph in one batched submission and removing an unnecessary wait on in-order command lists. The first graph execution under device tracing is still slow; see :doc:`knownissues`.
* **Behavior change:** Add ``zeCommandQueueExecuteCommandLists`` to the ``PTI_API_CLASS_GPU_OPERATION_CORE`` API class, covering applications that submit work through non-immediate command lists. Applications that enable this class with ``ptiViewEnableDriverApiClass`` now receive additional ``PTI_VIEW_DRIVER_API`` records (and the associated overhead) for these calls.
* **Behavior change:** ``ptiViewOverheadKindToString`` now returns ``"Instrumentation"`` for ``PTI_VIEW_OVERHEAD_KIND_TIME`` (previously ``"BUFFER_TIME"``). Code that compares the returned string must be updated; compare the enum value where possible.
* Fix a stale external correlation ID being reported alongside the live one when an ID is popped inside a ``ptiCallback`` subscriber callback.
* Fix duplicate kernel records when tracing Level Zero graph creation with compute-runtime 26.31 and later.
* Add build support for the open-source DPC++/DPCLang compiler (DPC++ 7.0), Rocky Linux 8, and building PTI as a CMake subproject, and fix compilation errors with the Intel oneAPI Compiler 2026.2.

Version 1.1.0
---------------

**Updates in this release:**

* Add support for on-demand profiling of Level Zero native graphs, validated on Intel(R) Arc(TM) B-Series (BMG) with a single GPU. Requires Intel oneAPI DPC++/C++ Compiler 2026.1.1 and compute-runtime 1.15.39122+12 or newer.
* Update to Level Zero Loader version 1.32.0 and add the new Level Zero graph and SYCL Unified Runtime API IDs to ``pti_driver_levelzero_api_ids.h`` and ``pti_runtime_sycl_api_ids.h``.
* Intern kernel names to avoid unbounded memory growth in applications that launch many kernels.
* Keep demangled kernel names in aggregated ``ptiPcSampling`` results.
* Fix a data race on the buffer handoff between the thread producing records and the thread delivering buffers to the user.
* Remove the obsolete bridge kernel GPU tracing path; the ``PTI_FORCE_BRIDGE_COMMANDS`` environment variable no longer has any effect.
* Fix loading the PTI core library from the Python wheel: the dependencies it cannot find through its own RPATH are now pre-loaded by the PTI interface library. Fix builds with Intel oneAPI Compiler 2025.x and 2026.0.

Version 1.0.1
---------------

**Updates in this release:**

* Document that ``ptiPcSamplingEnable`` returns ``PTI_ERROR_PC_SAMPLING_UNSUPPORTED`` when PC sampling is not supported on the system.
* Fix undefined symbols when running with LTS drivers on Intel(R) Data Center GPU Max Series: Level Zero symbols newer than loader version 1.14.0 are loaded dynamically.
* Fix a crash on Windows when the PTI library is unloaded.
* Fix builds with newer Level Zero Loader headers and MSVC Debug builds, and reconfigure the build when the ``VERSION`` file changes.

Version 1.0.0
---------------

**Updates in this release:**

* Declare the ``ptiView`` API stable for the 1.0 release, with fixed 4-byte enum sizes for a stable ABI.
* Extend the ``pti_view_record_kernel``, ``pti_view_record_memory_copy``, ``pti_view_record_memory_copy_p2p``, and ``pti_view_record_memory_fill`` records with new device handle and engine (ordinal and index) fields, defined as the new ``_v2`` record structures ``pti_view_record_kernel_v2``, ``pti_view_record_memory_copy_v2``, ``pti_view_record_memory_copy_p2p_v2``, and ``pti_view_record_memory_fill_v2``. The previous records remain compatible with the new ones.
* Add a proof-of-concept of GPU PC Sampling via the new ``ptiPcSampling`` API (Experimental) for statistical per-instruction stall sampling on GPU kernels. The API is complete, but the functionality is not: specifically, "address to source line mapping" is not yet implemented and is expected in the next version.
* Add the new ``ptiCallback`` API (Experimental), providing synchronous notifications for GPU operation lifecycle events, with kernel handle context and driver/kernel callback domains.
* Extend ``ptiMetricsScope`` with multi-device support, including multi-process auto-mode collection running in parallel.
* Add an initial proof-of-concept for on-demand SYCL Graph profiling with reduced overhead for repeatedly executed graphs.
* Reduce host-side runtime collection overhead significantly, in part by switching to GPU-time interpolation. Further reduction is in progress.
* Fix device timestamp desynchronization that could produce incorrect kernel start/end times when the device submit time exceeded the device start time.
* Improve tracing of Intel(R) oneCCL library operations.
* Add compatibility with Intel oneAPI Compiler 2026.0 and 2026.1, the open-source DPC++ compiler, and newer Level Zero Loader headers, including the Level Zero driver extension headers.

Version 0.17.0
---------------

**Updates in this release:**

* Add support for tracing Intel(R) oneCCL library operations (via ``ptiViewEnable(PTI_VIEW_COMMUNICATION)``)
* Add tracing of SYCL Graph operations with capture of repeatedly executed command lists. Improvements of this capability are in progress.
* Add compatibility with Intel oneAPI Compiler 2026.
* Update to Level Zero Loader version 1.28.0.
