PTI Metrics Scope API Reference
################################

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

This section provides API documentation for the PTI Metrics Scope API (per-kernel hardware metrics collection).

.. note::
   This page documents the **Metrics Scope API** for per-kernel hardware metrics collection. For device-level metrics sampling, see :doc:`metrics`.

Overview
========

The **Metrics Scope API** enables per-kernel hardware metrics collection for Intel GPUs. This API correlates hardware performance counters with individual kernel executions.

The API includes:

* **Configuration Functions** - Enable metrics scope, configure devices and metrics
* **Collection Control Functions** - Start and stop metrics collection
* **Data Retrieval Functions** - Retrieve and process collected metrics data
* **Structures** - Data structures for metrics records and metadata
* **Enumerators** - Collection modes and value types
* **Type Definitions** - Handle types for scope collection

----

Detailed API Documentation
===========================

Configuration Functions
-----------------------

.. doxygenfunction:: ptiMetricsScopeEnable

.. doxygenfunction:: ptiMetricsScopeConfigure

.. doxygenfunction:: ptiMetricsScopeQueryCollectionBufferSize

.. doxygenfunction:: ptiMetricsScopeSetCollectionBufferSize

Collection Control Functions
----------------------------

.. doxygenfunction:: ptiMetricsScopeStartCollection

.. doxygenfunction:: ptiMetricsScopeStopCollection

Data Retrieval Functions
-------------------------

.. doxygenfunction:: ptiMetricsScopeGetCollectionBuffersCount

.. doxygenfunction:: ptiMetricsScopeGetCollectionBuffer

.. doxygenfunction:: ptiMetricsScopeGetCollectionBufferProperties

.. doxygenfunction:: ptiMetricsScopeQueryMetricsBufferSize

.. doxygenfunction:: ptiMetricsScopeCalculateMetrics

.. doxygenfunction:: ptiMetricsScopeGetMetricsMetadata

.. doxygenfunction:: ptiMetricsScopeDisable

Structures
----------

.. doxygenstruct:: pti_metrics_scope_record_t
   :members:

.. doxygenstruct:: pti_metrics_scope_record_metadata_t
   :members:

.. doxygenstruct:: pti_metrics_scope_collection_buffer_properties_t
   :members:

Enumerators
-----------

.. doxygenenum:: pti_metrics_scope_mode_t

Type Definitions
----------------

.. doxygentypedef:: pti_scope_collection_handle_t
