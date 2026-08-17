==============
What's New
==============

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Version 1.0.0
---------------

*Current development version*

**Updates in this release:**

* Declare the ``ptiView`` API stable for the 1.0 release, with fixed 4-byte enum sizes for a stable ABI.
* Extend the ``pti_view_record_kernel``, ``pti_view_record_memory_copy``, ``pti_view_record_memory_copy_p2p``, and ``pti_view_record_memory_fill`` records with new device handle and engine (ordinal and index) fields, defined as the new ``_v2`` record structures ``pti_view_record_kernel_v2``, ``pti_view_record_memory_copy_v2``, ``pti_view_record_memory_copy_p2p_v2``, and ``pti_view_record_memory_fill_v2``. The previous records remain compatible with the new ones.
* Add a proof-of-concept of GPU PC Sampling via the new ``ptiPcSampling`` API (Experimental) for statistical per-instruction stall sampling on GPU kernels. The API is complete, but the functionality is not: specifically, "address to source line mapping" is not yet implemented and is expected in the next version.
* Add the new ``ptiCallback`` API (Experimental), providing synchronous notifications for GPU operation lifecycle events, with kernel handle context and driver/kernel callback domains.
* Add the new ``ptiMetricsEnable`` and ``ptiMetricsDisable`` APIs, which enable metrics collection at runtime and remove the need to set the ``ZET_ENABLE_METRICS=1`` environment variable before the process starts. Calls are reference counted per device and must be matched. Two new ``pti_result`` codes report failures of these APIs: ``PTI_ERROR_METRICS_ENABLE_FAILED`` and ``PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED`` (returned when the installed driver does not support runtime enabling, in which case the environment variable is still required).
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
