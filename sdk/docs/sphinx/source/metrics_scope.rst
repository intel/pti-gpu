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

One MetricsScope collection can profile several devices at once, provided they all report the same ``_model_name`` in ``pti_device_properties_t``. Pass an explicit list of devices to ``ptiMetricsScopeConfigure``, or pass none to profile every device the workload uses. When no devices are passed, the same-model check covers every device returned by ``ptiMetricsGetDevices``, not only the ones the workload uses, so on a system with mixed GPU models pass an explicit list of same-model devices (use ``ptiMetricsGetDevices`` to group the handles by model). Each collection buffer holds data for a single device; use ``ptiMetricsScopeGetCollectionBufferProperties`` to find which one. ``ptiMetricsScopeEnable`` enables metrics on all devices itself (see ``ptiMetricsEnable`` in :doc:`metrics`).

A Level Zero GPU view kind (for example ``PTI_VIEW_DEVICE_GPU_KERNEL``; see ``ptiCallbackSubscribe`` in :doc:`callback_api` for the list) must be enabled with ``ptiViewEnable`` before ``ptiMetricsScopeStartCollection``; otherwise it returns ``PTI_ERROR_NO_GPU_VIEWS_ENABLED``.

Metric values are returned as ``pti_value_t``, typed by ``pti_metric_value_type``; see :doc:`metrics`.

The API includes:

* **Configuration Functions** - Enable metrics scope, configure devices and metrics
* **Collection Control Functions** - Start and stop metrics collection
* **Data Retrieval Functions** - Retrieve and process collected metrics data
* **Structures** - Data structures for metrics records and metadata
* **Enumerators** - Collection modes
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
