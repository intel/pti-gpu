PTI Callback API Reference (Experimental)
##########################################

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

.. warning::
   The Callback API is **EXPERIMENTAL** and subject to change. APIs and data structures may change in future releases without notice. Use with caution in production environments.

This section provides API documentation for the PTI Callback API (synchronous GPU operation notifications).

.. note::
   Most users should use the :doc:`view_api_ref` (View API) instead. The Callback API is only necessary when you need synchronous callbacks during GPU operation lifecycle events.

Overview
========

The PTI Callback API provides a low-level subscription mechanism for receiving synchronous notifications about GPU operations as they are appended, dispatched, or completed. Unlike the View API which delivers buffered profiling records, the Callback API delivers real-time callbacks during specific GPU operation lifecycle events.

The Callback API is primarily designed for:

* **Internal PTI infrastructure** - Used internally by :doc:`metrics_scope` for per-kernel metrics collection
* **Advanced profiling tools** - Tools requiring immediate notification of GPU operations
* **Custom correlation systems** - Applications needing to inject custom correlation IDs at precise moments
* **Low-latency monitoring** - Systems requiring minimal delay between operation and notification

The API includes:

* **Core Functions** - Subscribe/unsubscribe, enable/disable callback domains
* **Helper Functions** - Convert callback domain and phase enums to strings
* **Structures** - Callback data structures for GPU operations
* **Enumerators** - Callback domains, phases, operation kinds
* **Type Definitions** - Subscriber handles and callback function pointers

----

Detailed API Documentation
===========================

Core Functions
--------------

.. doxygenfunction:: ptiCallbackSubscribe

.. doxygenfunction:: ptiCallbackUnsubscribe

.. doxygenfunction:: ptiCallbackEnableDomain

.. doxygenfunction:: ptiCallbackDisableDomain

.. doxygenfunction:: ptiCallbackDisableAllDomains

Helper Functions
----------------

.. doxygenfunction:: ptiCallbackDomainTypeToString

.. doxygenfunction:: ptiCallbackPhaseTypeToString

Structures
----------

.. doxygenstruct:: pti_callback_gpu_op_data
   :members:

.. doxygenstruct:: pti_gpu_op_details
   :members:

.. doxygenstruct:: pti_internal_callback_data
   :members:

Enumerators
-----------

.. doxygenenum:: pti_callback_domain

.. doxygenenum:: pti_callback_phase

.. doxygenenum:: pti_backend_command_list_type

.. doxygenenum:: pti_internal_event_type

.. doxygenenum:: pti_gpu_operation_kind

Type Definitions
----------------

.. doxygentypedef:: pti_callback_subscriber_handle

.. doxygentypedef:: pti_callback_function
