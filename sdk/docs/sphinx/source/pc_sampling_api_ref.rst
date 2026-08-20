PTI PC Sampling API Reference (Experimental)
############################################

.. warning::
   **EXPERIMENTAL API** - This API is experimental and subject to change without deprecation in future releases.

This section provides API documentation for the PTI PC Sampling API (GPU EU Stall Sampling).

Overview
========

The PTI PC Sampling API provides statistical per-instruction sampling of active execution and stall reasons on GPU kernels. This enables fine-grained performance analysis by identifying instruction-level bottlenecks and stall reasons.

The API includes:

* **Configuration Functions** - Enable, configure, and set buffer sizes
* **Collection Control Functions** - Start and stop sampling
* **Data Retrieval Functions** - Get devices, kernels, stall reasons, and sample data
* **Cleanup Functions** - Disable and free resources
* **Structures** - Kernel info, stall reasons, instruction data, device status
* **Type Definitions** - Handle types for PC sampling

Configuration Functions
=======================

* :ref:`ptiPcSamplingEnable <ptiPcSamplingEnable>` - Create a collection handle
* :ref:`ptiPcSamplingConfigure <ptiPcSamplingConfigure>` - Configure devices and sampling period (optional)
* :ref:`ptiPcSamplingQueryCollectionBufferSize <ptiPcSamplingQueryCollectionBufferSize>` - Query recommended buffer size
* :ref:`ptiPcSamplingSetCollectionBufferSize <ptiPcSamplingSetCollectionBufferSize>` - Set collection buffer size

.. note::
   ``ptiPcSamplingConfigure`` is optional and is only needed to override the defaults. When it is
   skipped, ``ptiPcSamplingStartCollection`` applies the default configuration: the device(s) PTI
   selects and the default sampling period of 100000 ns. The collection lifecycle is therefore
   ``ptiPcSamplingEnable`` → ``ptiPcSamplingConfigure`` (optional) → ``ptiPcSamplingStartCollection``
   → ``ptiPcSamplingStopCollection``.

Collection Control Functions
============================

* :ref:`ptiPcSamplingStartCollection <ptiPcSamplingStartCollection>` - Begin sampling
* :ref:`ptiPcSamplingStopCollection <ptiPcSamplingStopCollection>` - End sampling

Data Retrieval Functions
=========================

* :ref:`ptiPcSamplingGetStallReasons <ptiPcSamplingGetStallReasons>` - Get stall reason information
* :ref:`ptiPcSamplingGetProfiledDevices <ptiPcSamplingGetProfiledDevices>` - Get profiled device handles
* :ref:`ptiPcSamplingGetDeviceStatus <ptiPcSamplingGetDeviceStatus>` - Get per-device collection status
* :ref:`ptiPcSamplingGetObservedKernelHandles <ptiPcSamplingGetObservedKernelHandles>` - Get kernel handles with samples
* :ref:`ptiPcSamplingGetObservedKernelInfo <ptiPcSamplingGetObservedKernelInfo>` - Get kernel metadata and aggregated samples
* :ref:`ptiPcSamplingGetSamplesPerInstruction <ptiPcSamplingGetSamplesPerInstruction>` - Get instruction-level sampling data

Cleanup Functions
=================

* :ref:`ptiPcSamplingDisable <ptiPcSamplingDisable>` - Free all resources

Structures
==========

Data structures for PC sampling:

* :ref:`pti_pc_sampling_stall_reason_info_t <pti_pc_sampling_stall_reason_info_t>` - Stall reason name and description
* :ref:`pti_pc_sampling_source_info_t <pti_pc_sampling_source_info_t>` - Source file and line information
* :ref:`pti_pc_sampling_instruction_t <pti_pc_sampling_instruction_t>` - Per-instruction sampling metrics
* :ref:`pti_pc_sampling_kernel_info_t <pti_pc_sampling_kernel_info_t>` - Kernel information and aggregated samples
* :ref:`pti_pc_sampling_device_status_t <pti_pc_sampling_device_status_t>` - Per-device collection status

.. note::
   The ``pti_pc_sampling_handle_t`` is an opaque handle type used to identify a PC sampling session. It is created by ``ptiPcSamplingEnable()`` and destroyed by ``ptiPcSamplingDisable()``.

----

Detailed API Documentation
===========================

Configuration Functions
-----------------------

.. _ptiPcSamplingEnable:
.. doxygenfunction:: ptiPcSamplingEnable

.. _ptiPcSamplingConfigure:
.. doxygenfunction:: ptiPcSamplingConfigure

.. _ptiPcSamplingQueryCollectionBufferSize:
.. doxygenfunction:: ptiPcSamplingQueryCollectionBufferSize

.. _ptiPcSamplingSetCollectionBufferSize:
.. doxygenfunction:: ptiPcSamplingSetCollectionBufferSize

Collection Control Functions
----------------------------

.. _ptiPcSamplingStartCollection:
.. doxygenfunction:: ptiPcSamplingStartCollection

.. _ptiPcSamplingStopCollection:
.. doxygenfunction:: ptiPcSamplingStopCollection

Data Retrieval Functions
-------------------------

.. _ptiPcSamplingGetStallReasons:
.. doxygenfunction:: ptiPcSamplingGetStallReasons

.. _ptiPcSamplingGetProfiledDevices:
.. doxygenfunction:: ptiPcSamplingGetProfiledDevices

.. _ptiPcSamplingGetDeviceStatus:
.. doxygenfunction:: ptiPcSamplingGetDeviceStatus

.. _ptiPcSamplingGetObservedKernelHandles:
.. doxygenfunction:: ptiPcSamplingGetObservedKernelHandles

.. _ptiPcSamplingGetObservedKernelInfo:
.. doxygenfunction:: ptiPcSamplingGetObservedKernelInfo

.. _ptiPcSamplingGetSamplesPerInstruction:
.. doxygenfunction:: ptiPcSamplingGetSamplesPerInstruction

Cleanup Functions
-----------------

.. _ptiPcSamplingDisable:
.. doxygenfunction:: ptiPcSamplingDisable

Structures
----------

.. _pti_pc_sampling_stall_reason_info_t:
.. doxygenstruct:: pti_pc_sampling_stall_reason_info_t
   :members:

.. _pti_pc_sampling_source_info_t:
.. doxygenstruct:: pti_pc_sampling_source_info_t
   :members:

.. _pti_pc_sampling_instruction_t:
.. doxygenstruct:: pti_pc_sampling_instruction_t
   :members:

.. _pti_pc_sampling_kernel_info_t:
.. doxygenstruct:: pti_pc_sampling_kernel_info_t
   :members:

.. _pti_pc_sampling_device_status_t:
.. doxygenstruct:: pti_pc_sampling_device_status_t
   :members:
