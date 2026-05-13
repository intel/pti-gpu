#####################
Introduction
#####################

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

PTI SDK is a software package for developing profiling tools for applications built on oneAPI and running on Intel GPUs.

The SDK provides the ``pti_view`` library with a set of APIs defined in header files, ``pti`` module, samples and tests.
These APIs allow a user to trace various host and device operations, such as GPU kernel executions, memory transfers, and runtime/driver calls, and to collect GPU hardware performance metrics. The library is designed to be used by profiling tools to analyze the performance of applications running on Intel GPUs.
The library implementation uses low-level tracing APIs of SYCL runtime and Level-Zero.

PTI SDK provides the following groups of APIs:

* ``ptiView`` - to trace GPU device kernels and memory transfers, host runtime and driver function calls, and deliver the data about traced instances, while the application runs, via registered callbacks in batches;
* ``ptiMetricsScope`` - to collect and annotate GPU kernel instances with GPU hardware performance metrics, such as EU utilization, memory bandwidth, cache statistics, and others;
* ``ptiMetrics`` - to discover available GPU hardware performance metrics and metrics sets, and collect them over time;
* ``ptiCallback`` - to subscribe to the notifications about GPU resources and operation lifecycle events (append, dispatch, complete) with a low-level synchronous callback API.

PTI SDK supports **on-demand** profiling for most use cases - the application can start and stop profiling with ``ptiViewEnable()`` and ``ptiViewDisable()`` calls correspondingly.
Outside the profiled region(s), PTI SDK incurs zero overhead.
``ptiView`` API, among other things, allows users to correlate GPU activities with application-level operations via external correlation IDs,
which helps to connect GPU activities with higher-level application logic and analyze performance in the context of the application behavior.

For ``ptiView`` the user controls the buffers management for the profiling records delivery, and can choose to flush the buffers at any time with ``ptiViewFlush()`` to get the collected data delivered
to the registered callbacks.

For the APIs whose profiling results require significant post-processing, such as ``ptiMetricsScope`` and ``ptiMetrics``, the results delivery is done as the post-collection step.


Getting Started with PTI SDK
=============================

There are two ways to use PTI SDK:

**Using Pre-built Binaries**
  Pre-built binaries are available from PyPI. See :doc:`build_and_install` for installation instructions and :doc:`linking` for instructions on how to link PTI SDK in your application.

**Building from Source**
  To build PTI SDK from source, follow the build instructions in :doc:`build_and_install`. The build process will create the necessary libraries and sample executables.

Next Steps
==========

After installation (either from binaries or by building from source):

1. Review the :doc:`quickstart` guide for an introduction
2. Explore the samples in the ``samples/`` directory - see :doc:`samples` for descriptions
3. Check out the API documentation:

   * :doc:`view_api_ref` - PTI View API for tracing GPU operations
   * :doc:`metrics_scope` - Metrics Scope API for per-kernel hardware metrics
   * :doc:`metrics` - Device-level metrics API for continuous sampling
   * :doc:`callback_api` - Callback API for synchronous GPU operation notifications (Experimental)

