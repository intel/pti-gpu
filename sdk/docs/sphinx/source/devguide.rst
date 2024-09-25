Developer's Guide
###################

TODO: Add general description of how to use the PTI API in application code.

PTI Library C API version |release| Reference
===============================================

* Functions

  * :ref:`ptiViewSetCallbacks <ptiViewSetCallbacks>`
  * :ref:`ptiViewEnable <ptiViewEnable>`
  * :ref:`ptiViewDisable <ptiViewDisable>`
  * :ref:`ptiViewGPULocalAvailable <ptiViewGPULocalAvailable>`
  * :ref:`ptiViewFlushAllViews <ptiViewFlushAllViews>`
  * :ref:`ptiViewGetNextRecord <ptiViewGetNextRecord>`
  * :ref:`ptiViewPushExternalCorrelationId <ptiViewPushExternalCorrelationId>`
  * :ref:`ptiViewPopExternalCorrelationId <ptiViewPopExternalCorrelationId>`
  * :ref:`ptiViewGetTimestamp <ptiViewGetTimestamp>`
  * :ref:`ptiViewSetTimestampCallback <ptiViewSetTimestampCallback>`

* Helper Functions

  * :ref:`ptiViewOverheadKindToString <ptiViewOverheadKindToString>`
  * :ref:`ptiViewMemoryTypeToString <ptiViewMemoryTypeToString>`
  * :ref:`ptiViewMemcpyTypeToString <ptiViewMemcpyTypeToString>`

* Structures

  * :ref:`pti_view_record_base  <pti_view_record_base>`
  * :ref:`pti_view_record_sycl_runtime  <pti_view_record_sycl_runtime>`
  * :ref:`pti_view_record_kernel  <pti_view_record_kernel>`
  * :ref:`pti_view_record_memory_copy  <pti_view_record_memory_copy>`
  * :ref:`pti_view_record_memory_copy_p2p  <pti_view_record_memory_copy_p2p>`
  * :ref:`pti_view_record_memory_fill  <pti_view_record_memory_fill>`
  * :ref:`pti_view_record_external_correlation  <pti_view_record_external_correlation>`
  * :ref:`pti_view_record_overhead  <pti_view_record_overhead>`

* Enumerators

  * :ref:`pti_result <pti_result>`
  * :ref:`pti_view_kind <pti_view_kind>`
  * :ref:`pti_view_external_kind  <pti_view_external_kind>`
  * :ref:`pti_view_overhead_kind  <pti_view_overhead_kind>`
  * :ref:`pti_view_memory_type  <pti_view_memory_type>`
  * :ref:`pti_view_memcpy_type  <pti_view_memcpy_type>`

* Function Pointer Typedefs

  * :ref:`pti_fptr_buffer_completed  <pti_fptr_buffer_completed>`
  * :ref:`pti_fptr_buffer_requested  <pti_fptr_buffer_requested>`

Functions
-------------

.. _ptiViewSetCallbacks:
.. doxygenfunction:: ptiViewSetCallbacks

.. _ptiViewEnable:
.. doxygenfunction:: ptiViewEnable

.. _ptiViewDisable:
.. doxygenfunction:: ptiViewDisable

.. _ptiViewGPULocalAvailable:
.. doxygenfunction:: ptiViewGPULocalAvailable

.. _ptiViewFlushAllViews:
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

Helper Functions
-------------------

.. _ptiViewOverheadKindToString:
.. doxygenfunction:: ptiViewOverheadKindToString

.. _ptiViewMemoryTypeToString:
.. doxygenfunction:: ptiViewMemoryTypeToString

.. _ptiViewMemcpyTypeToString:
.. doxygenfunction:: ptiViewMemcpyTypeToString

Structures
-------------

.. _pti_view_record_base:
.. doxygenstruct::   pti_view_record_base
   :members:
      
.. _pti_view_record_sycl_runtime:
.. doxygenstruct::   pti_view_record_sycl_runtime
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

.. _pti_view_record_external_correlation:
.. doxygenstruct::   pti_view_record_external_correlation
   :members:

.. _pti_view_record_overhead:
.. doxygenstruct::   pti_view_record_overhead
   :members:

Enumerators
-------------
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


Function Pointer Typedefs
--------------------------

.. _pti_fptr_buffer_completed:
.. doxygentypedef:: pti_fptr_buffer_completed

.. _pti_fptr_buffer_requested:
.. doxygentypedef:: pti_fptr_buffer_requested



