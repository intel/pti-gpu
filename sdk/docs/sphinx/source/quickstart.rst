============================
Quick Start with ptiView API
============================

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

This guide will help you get started with PTI SDK. By the end, you'll be able to trace GPU kernels and understand the basic API workflow.

Prerequisites
-------------

Before starting, ensure PTI SDK is installed. See :doc:`build_and_install` for installation instructions.

Environment Setup
-----------------

Set up the Intel(R) oneAPI environment:

**Linux:**

.. code-block:: bash

   source <path_to_oneapi>/setvars.sh

**Windows:**

Open the Intel(R) oneAPI Command Prompt.

Basic Usage Pattern
-------------------

The ``ptiView`` API follows this pattern:

1. **Define callbacks** for buffer allocation and data processing
2. **Register callbacks** with ``ptiViewSetCallbacks()``
3. **Enable tracing** with ``ptiViewEnable()``
4. **Run your application**
5. **Disable tracing** with ``ptiViewDisable()``
6. **Flush remaining data** with ``ptiFlushAllViews()``

Running Your First Sample
--------------------------

Let's run the vector square-add sample to see PTI SDK in action.

**Step 1: Navigate to the build directory**

.. code-block:: bash

   cd build

**Step 2: Run the sample**

**Linux:**

.. code-block:: bash

   ./bin/vec_sqadd

**Windows:**

.. code-block:: batch

   bin\vec_sqadd.exe

Understanding the Output
------------------------

The sample will display:

1. **Device Information**: GPU device being used
2. **Kernel Traces**: Information about GPU kernels executed
3. **Memory Operations**: Data transfers between host and device
4. **Timing Information**: Timestamps and durations

Example output:

.. code-block:: text

   >>>> [123456789] zeKernelCreate: zeKernel = 0xdeadbeef
   >>>> [123456790] zeCommandListAppendLaunchKernel
   Kernel: VecSq, Duration: 1234 ns
   >>>> [123456800] zeCommandListAppendMemoryCopy
   Memory Copy: Host->Device, Size: 20000 bytes, Duration: 567 ns

Basic API Example
-----------------

Here's a minimal example showing the PTI SDK API usage:

.. code-block:: c++

   #include "pti/pti_view.h"
   #include <iostream>

   // Callback to allocate buffer
   void BufferRequested(unsigned char** buf, size_t* buf_size) {
       *buf_size = 1024 * 1024;  // 1 MB
       *buf = new unsigned char[*buf_size];
   }

   // Callback to process collected data
   void BufferCompleted(unsigned char* buf, size_t buf_size, size_t valid_buf_size) {
       if (!buf || !valid_buf_size) return;

       pti_view_record_base* ptr = nullptr;
       while (true) {
           auto status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
           if (status == PTI_STATUS_END_OF_BUFFER) break;
           if (status != PTI_SUCCESS) {
               std::cerr << "Error reading record" << std::endl;
               break;
           }

           if (ptr->_view_kind == PTI_VIEW_DEVICE_GPU_KERNEL) {
               auto* kernel = reinterpret_cast<pti_view_record_kernel*>(ptr);
               std::cout << "Kernel: " << kernel->_name
                         << ", Duration: " << (kernel->_end_timestamp - kernel->_start_timestamp)
                         << " ns" << std::endl;
           }
       }
       delete[] buf;
   }

   int main() {
       // 1. Register callbacks for buffer allocation and data processing
       ptiViewSetCallbacks(BufferRequested, BufferCompleted);

       // 2. Enable tracing for kernels
       ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL);

       // 3. Run your SYCL/Level-Zero application
       // ... your GPU workload here ...

       // 4. Disable tracing
       ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);

       // 5. Flush any remaining buffered data
       ptiFlushAllViews();

       return 0;
   }

Available Tracing Views
-----------------------

PTI SDK supports tracing different types of activities:

**Device Operations:**

* ``PTI_VIEW_DEVICE_GPU_KERNEL`` - GPU kernel execution on the device
* ``PTI_VIEW_DEVICE_GPU_MEM_COPY`` - Memory copy operations between host and device
* ``PTI_VIEW_DEVICE_GPU_MEM_FILL`` - Memory fill operations on the device
* ``PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P`` - Peer-to-peer memory copies between devices
* ``PTI_VIEW_DEVICE_SYNCHRONIZATION`` - Synchronization operations on host and GPU (barriers, fences, events)

**API Tracing:**

* ``PTI_VIEW_RUNTIME_API`` - Runtime API calls (SYCL)
* ``PTI_VIEW_DRIVER_API`` - Driver/back-end API calls (Level-Zero)

**Profiling Support:**

* ``PTI_VIEW_EXTERNAL_CORRELATION`` - Application-level correlation IDs for connecting GPU activities with user annotations
* ``PTI_VIEW_COLLECTION_OVERHEAD`` - Profiling overhead tracking

**Communication:**

* ``PTI_VIEW_COMMUNICATION`` - Communication operations via Intel® oneCCL (Linux only)

You can enable multiple views simultaneously:

.. code-block:: c++

   ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL);
   ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
   ptiViewEnable(PTI_VIEW_RUNTIME_API);

Running Tests
-------------

To verify your PTI SDK installation, run the test suite:

**Linux:**

.. code-block:: bash

   cd build
   make test

Or with CTest for detailed output:

.. code-block:: bash

   ctest --output-on-failure

**Windows:**

.. code-block:: batch

   cd build
   ninja test

Or with CTest:

.. code-block:: batch

   ctest --output-on-failure

On-Demand Collection
---------------------

PTI SDK supports **on-demand** profiling for zero overhead outside the profiled regions.
You can enable and disable tracing around specific code sections to focus on areas of interest:

.. code-block:: c++

   // Application initialization
   setup_callbacks();

   // No overhead here - tracing is not enabled
   some_work();

   // Start tracing - this is where collection begins
   ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL);

   // Trace this section
   important_work();

   // Stop tracing - zero overhead resumes
   ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);

   // Flush remaining buffered data
   ptiFlushAllViews();

   // No overhead here again
   more_work();

Next Steps
----------

Now that you've run your first sample, explore more:

1. **Examine the samples** in ``samples/`` directory:

   * ``vector_sq_add`` - Basic tracing
   * ``dpc_gemm`` - GEMM with performance tracking
   * ``callback`` - Advanced callback usage
   * ``metrics_scope`` - Hardware metrics collection

2. **Check the API Reference** for detailed documentation:

   * :doc:`view_api_ref` - Complete PTI View API for tracing
   * :doc:`metrics_scope` - Per-kernel hardware metrics collection
   * :doc:`metrics` - Device-level metrics collection
   * :doc:`callback_api` - Advanced callback patterns
   * Function signatures
   * Data structure definitions

4. **Browse sample code** at ``samples/`` for real-world usage patterns

Troubleshooting
---------------

**No output from sample:**
   - Ensure oneAPI environment is set up (``setvars.sh``)
   - Verify GPU drivers are installed
   - Check that Level-Zero loader is available

**Callbacks not called:**
   - Ensure ``ptiViewSetCallbacks()`` is called **before** ``ptiViewEnable()``
   - Verify callbacks are properly registered
   - Check that you're enabling the correct view types

**Build errors:**
   - See the :doc:`build_and_install` guide for build requirements
   - Ensure C++17 support is available

**Performance issues:**
   - Use on-demand collection (``ptiViewEnable``/``ptiViewDisable``) to reduce overhead
   - Consider reducing callback complexity
   - Profile only regions of interest

For more help, see the `GitHub repository <https://github.com/intel/pti-gpu>`_ or submit an issue.



