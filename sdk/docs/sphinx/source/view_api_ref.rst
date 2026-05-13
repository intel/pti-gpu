PTI View API Reference
######################

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

This section provides API documentation for the PTI View API (tracing and profiling).

.. note::
   This page documents the **PTI View API** for tracing GPU activities. For hardware metrics collection, see :doc:`metrics_scope` and :doc:`metrics`.

Overview
========

The PTI View API provides tracing capabilities for GPU operations. The API includes:

* **Core Functions** - Enable/disable tracing, set callbacks, manage views
* **Helper Functions** - Utility functions for string conversion
* **Structures** - Data structures for profiling records
* **Enumerators** - View types, result codes, memory types
* **Function Pointers** - Callback function signatures

Core Functions
==============

* :ref:`ptiViewSetCallbacks <ptiViewSetCallbacks>` - Register callback functions
* :ref:`ptiViewEnable <ptiViewEnable>` - Enable a profiling view
* :ref:`ptiViewDisable <ptiViewDisable>` - Disable a profiling view
* :ref:`ptiViewGPULocalAvailable <ptiViewGPULocalAvailable>` - Check on-demand collection support
* :ref:`ptiFlushAllViews <ptiFlushAllViews>` - Flush all pending records
* :ref:`ptiViewGetNextRecord <ptiViewGetNextRecord>` - Retrieve next record
* :ref:`ptiViewPushExternalCorrelationId <ptiViewPushExternalCorrelationId>` - Push correlation ID
* :ref:`ptiViewPopExternalCorrelationId <ptiViewPopExternalCorrelationId>` - Pop correlation ID
* :ref:`ptiViewGetTimestamp <ptiViewGetTimestamp>` - Get current timestamp
* :ref:`ptiViewSetTimestampCallback <ptiViewSetTimestampCallback>` - Set custom timestamp function
* :ref:`ptiViewGetApiIdName <ptiViewGetApiIdName>` - Get API name from ID
* :ref:`ptiViewEnableDriverApi <ptiViewEnableDriverApi>` - Enable/disable specific driver API
* :ref:`ptiViewEnableRuntimeApi <ptiViewEnableRuntimeApi>` - Enable/disable specific runtime API
* :ref:`ptiViewEnableDriverApiClass <ptiViewEnableDriverApiClass>` - Enable/disable driver API class
* :ref:`ptiViewEnableRuntimeApiClass <ptiViewEnableRuntimeApiClass>` - Enable/disable runtime API class

Helper Functions
================

* :ref:`ptiViewOverheadKindToString <ptiViewOverheadKindToString>` - Convert overhead kind to string
* :ref:`ptiViewMemoryTypeToString <ptiViewMemoryTypeToString>` - Convert memory type to string
* :ref:`ptiViewMemcpyTypeToString <ptiViewMemcpyTypeToString>` - Convert memcpy type to string

Structures
==========

Record structures contain profiling data passed to callbacks:

* :ref:`pti_view_record_base <pti_view_record_base>` - Base record structure
* :ref:`pti_view_record_api <pti_view_record_api>` - Runtime API call record
* :ref:`pti_view_record_kernel <pti_view_record_kernel>` - GPU kernel record
* :ref:`pti_view_record_memory_copy <pti_view_record_memory_copy>` - Memory copy record
* :ref:`pti_view_record_memory_copy_p2p <pti_view_record_memory_copy_p2p>` - Peer-to-peer copy record
* :ref:`pti_view_record_memory_fill <pti_view_record_memory_fill>` - Memory fill record
* :ref:`pti_view_record_synchronization <pti_view_record_synchronization>` - Synchronization record
* :ref:`pti_view_record_external_correlation <pti_view_record_external_correlation>` - External correlation record
* :ref:`pti_view_record_overhead <pti_view_record_overhead>` - Overhead record
* :ref:`pti_view_record_comms <pti_view_record_comms>` - Communication record (oneCCL, Linux only)

Enumerators
===========

* :ref:`pti_result <pti_result>` - Function return status codes
* :ref:`pti_view_kind <pti_view_kind>` - View type identifiers
* :ref:`pti_view_external_kind <pti_view_external_kind>` - External correlation types
* :ref:`pti_view_overhead_kind <pti_view_overhead_kind>` - Overhead types
* :ref:`pti_view_memory_type <pti_view_memory_type>` - Memory location types
* :ref:`pti_view_memcpy_type <pti_view_memcpy_type>` - Memory copy direction types
* :ref:`pti_view_synchronization_type <pti_view_synchronization_type>` - Synchronization operation types
* :ref:`pti_api_group_id <pti_api_group_id>` - API group identifiers (Level-Zero, SYCL, OpenCL)
* :ref:`pti_api_class <pti_api_class>` - API class categories for filtering

Function Pointer Typedefs
==========================

Callback function signatures:

* :ref:`pti_fptr_buffer_completed <pti_fptr_buffer_completed>` - Buffer completed callback
* :ref:`pti_fptr_buffer_requested <pti_fptr_buffer_requested>` - Buffer requested callback

----

Detailed API Documentation
===========================

Core Functions
--------------

.. _ptiViewSetCallbacks:
.. doxygenfunction:: ptiViewSetCallbacks

.. _ptiViewEnable:
.. doxygenfunction:: ptiViewEnable

.. _ptiViewDisable:
.. doxygenfunction:: ptiViewDisable

.. _ptiViewGPULocalAvailable:
.. doxygenfunction:: ptiViewGPULocalAvailable

.. _ptiFlushAllViews:
.. doxygenfunction:: ptiFlushAllViews

.. _ptiViewGetNextRecord:
.. doxygenfunction:: ptiViewGetNextRecord

.. _ptiViewPushExternalCorrelationId:
.. doxygenfunction:: ptiViewPushExternalCorrelationId

.. _ptiViewPopExternalCorrelationId:
.. doxygenfunction:: ptiViewPopExternalCorrelationId

.. _ptiViewGetTimestamp:
.. doxygenfunction:: ptiViewGetTimestamp

.. _ptiViewSetTimestampCallback:
.. doxygenfunction:: ptiViewSetTimestampCallback

.. _ptiViewGetApiIdName:
.. doxygenfunction:: ptiViewGetApiIdName

.. _ptiViewEnableDriverApi:
.. doxygenfunction:: ptiViewEnableDriverApi

.. _ptiViewEnableRuntimeApi:
.. doxygenfunction:: ptiViewEnableRuntimeApi

.. _ptiViewEnableDriverApiClass:
.. doxygenfunction:: ptiViewEnableDriverApiClass

.. _ptiViewEnableRuntimeApiClass:
.. doxygenfunction:: ptiViewEnableRuntimeApiClass

Helper Functions
----------------

.. _ptiViewOverheadKindToString:
.. doxygenfunction:: ptiViewOverheadKindToString

.. _ptiViewMemoryTypeToString:
.. doxygenfunction:: ptiViewMemoryTypeToString

.. _ptiViewMemcpyTypeToString:
.. doxygenfunction:: ptiViewMemcpyTypeToString

Structures
----------

.. _pti_view_record_base:
.. doxygenstruct::   pti_view_record_base
   :members:

.. _pti_view_record_api:
.. doxygenstruct::   pti_view_record_api
   :members:

.. _pti_view_record_kernel:
.. doxygenstruct::   pti_view_record_kernel
   :members:

.. _pti_view_record_memory_copy:
.. doxygenstruct::   pti_view_record_memory_copy
   :members:

.. _pti_view_record_memory_copy_p2p:
.. doxygenstruct::   pti_view_record_memory_copy_p2p
   :members:

.. _pti_view_record_memory_fill:
.. doxygenstruct::   pti_view_record_memory_fill
   :members:

.. _pti_view_record_synchronization:
.. doxygenstruct::   pti_view_record_synchronization
   :members:

.. _pti_view_record_external_correlation:
.. doxygenstruct::   pti_view_record_external_correlation
   :members:

.. _pti_view_record_overhead:
.. doxygenstruct::   pti_view_record_overhead
   :members:

.. _pti_view_record_comms:
.. doxygenstruct::   pti_view_record_comms
   :members:

Enumerators
-----------

.. _pti_result:
.. doxygenenum:: pti_result

.. _pti_view_kind:
.. doxygenenum:: pti_view_kind

.. _pti_view_external_kind:
.. doxygenenum:: pti_view_external_kind

.. _pti_view_overhead_kind:
.. doxygenenum:: pti_view_overhead_kind

.. _pti_view_memory_type:
.. doxygenenum:: pti_view_memory_type

.. _pti_view_memcpy_type:
.. doxygenenum:: pti_view_memcpy_type

.. _pti_view_synchronization_type:
.. doxygenenum:: pti_view_synchronization_type

.. _pti_api_group_id:
.. doxygenenum:: pti_api_group_id

.. _pti_api_class:
.. doxygenenum:: pti_api_class

Function Pointer Typedefs
--------------------------

.. _pti_fptr_buffer_completed:
.. doxygentypedef:: pti_fptr_buffer_completed

.. _pti_fptr_buffer_requested:
.. doxygentypedef:: pti_fptr_buffer_requested
