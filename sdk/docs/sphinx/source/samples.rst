============
Code Samples
============

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Sample applications demonstrate how to use PTI SDK tracing and profiling capabilities. All samples are located in the ``samples/`` directory of the repository.

**Repository Location:**

* Local: ``sdk/samples/``
* GitHub: https://github.com/intel/pti-gpu/tree/master/sdk/samples


``ptiView`` API Samples
~~~~~~~~~~~~~~~~~~~~~~~

Demonstrates how to use the :doc:`ptiView API <view_api_ref>` for tracing GPU operations, including kernel execution, memory copies, and API calls.

.. list-table::
   :widths: 20 35 45
   :header-rows: 1

   * - Sample
     - Description
     - ``PTI_VIEW_...``
   * - **vector_sq_add**
     - SYCL vector square-add kernel
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``RUNTIME_API``, ``EXTERNAL_CORRELATION``
   * - **dpc_gemm**
     - DPC++ matrix multiplication with comprehensive tracing
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``RUNTIME_API``, ``DRIVER_API``, ``EXTERNAL_CORRELATION``, ``COLLECTION_OVERHEAD``
   * - **onemkl_gemm**
     - Matrix multiplication using Intel(R) oneMKL library
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``RUNTIME_API``
   * - **itt_ccl**
     - Intel(R) oneCCL library operations tracing (Linux only)
     - ``COMMUNICATION``
   * - **dpc_gemm_threaded**
     - Multi-threaded DPC++ matrix multiplication
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``RUNTIME_API``, ``DRIVER_API``, ``COLLECTION_OVERHEAD``
   * - **omp_vec_add**
     - C-language OpenMP sample with GPU offload
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``DRIVER_API``
   * - **view_record_versioned**
     - Version-aware tracing that handles v1 and v2 view record structures using ``ptiVersion()``
     - ``DEVICE_GPU_KERNEL``, ``DEVICE_GPU_MEM_COPY``, ``DEVICE_GPU_MEM_FILL``, ``RUNTIME_API``, ``DRIVER_API``, ``COLLECTION_OVERHEAD``

``ptiCallback`` API Sample
~~~~~~~~~~~~~~~~~~~~~~~~~~

**callback**
   Demonstrates the experimental :doc:`ptiCallback API <callback_api>` for synchronous GPU operation notifications.

   .. warning::
      The ``ptiCallback`` API is experimental and subject to change. See :doc:`callback_api` for documentation.

GPU Hardware Metrics Collection Samples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**metrics_scope**
   Per-kernel hardware metrics collection using :doc:`ptiMetricsScope API <metrics_scope>`.

**metrics_perf**
   Device-level time-based metrics sampling using the :doc:`ptiMetrics API <metrics>`. Use the ``--list-metrics`` option to output all available metrics sets and metrics on the system GPUs.

**metrics_iso3dfd_dpcpp**
   Hardware metrics collection in ISO3DFD stencil computation workload.


Building Samples
================

Samples are built automatically with PTI SDK. See :doc:`build_and_install` for build instructions.

From the SDK build directory:

**Linux:**

.. code-block:: bash

   ./build/bin/vector_sq_add
   ./build/bin/dpc_gemm
   ./build/bin/metrics_scope

**Windows:**

.. code-block:: batch

   build\bin\vector_sq_add.exe
   build\bin\dpc_gemm.exe
   build\bin\metrics_scope.exe

Next Steps
==========

* :doc:`view_api_ref` - PTI View API for tracing
* :doc:`metrics_scope` - Per-kernel metrics collection
* :doc:`metrics` - Device-level metrics collection
* :doc:`callback_api` - Callback API documentation
