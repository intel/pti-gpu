PTI Metrics API Reference
##########################

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

This section provides API documentation for the PTI Metrics API (device-level metrics sampling).

.. note::
   This page documents the **Metrics API** for device-level time-based and event-based metrics sampling. For per-kernel metrics collection, see :doc:`metrics_scope`.

Overview
========

The **Metrics API** provides time-based and event-based sampling of hardware performance metrics across application runtime. This API provides system-level performance monitoring without kernel-level granularity.

The API includes:

* **Enable/Disable Functions** - Turn metrics collection on and off on the devices at runtime
* **Device Discovery Functions** - Enumerate devices that support metrics collection
* **Metric Discovery Functions** - Discover available metric groups and individual metrics
* **Collection Control Functions** - Configure, start, stop, pause, and resume collection
* **Data Retrieval Functions** - Retrieve calculated metrics data
* **Structures** - Device properties, metric properties, and collection parameters
* **Enumerators** - Metric group types, metric types, and value types

----

Detailed API Documentation
===========================

Enable/Disable Functions
------------------------

``ptiMetricsEnable`` must be called before any other Metrics API function. It replaces the need to set
the ``ZET_ENABLE_METRICS=1`` environment variable before the process starts, provided the installed
driver supports enabling metrics at runtime; otherwise it returns
``PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED`` and the environment variable must be used instead.

Calls are reference counted per device, so every ``ptiMetricsEnable`` call must be matched with a
``ptiMetricsDisable`` call. Metrics are disabled on a device only once its last reference is released.

.. doxygenfunction:: ptiMetricsEnable

.. doxygenfunction:: ptiMetricsDisable

Device Discovery Functions
--------------------------

.. doxygenfunction:: ptiMetricsGetDevices

Metric Discovery Functions
--------------------------

.. doxygenfunction:: ptiMetricsGetMetricGroups

.. doxygenfunction:: ptiMetricsGetMetricsProperties

Collection Control Functions
----------------------------

.. doxygenfunction:: ptiMetricsConfigureCollection

.. doxygenfunction:: ptiMetricsStartCollection

.. doxygenfunction:: ptiMetricsStartCollectionPaused

.. doxygenfunction:: ptiMetricsPauseCollection

.. doxygenfunction:: ptiMetricsResumeCollection

.. doxygenfunction:: ptiMetricsStopCollection

Data Retrieval Functions
-------------------------

.. doxygenfunction:: ptiMetricsGetCalculatedData

Structures
----------

.. doxygenstruct:: pti_device_properties_t
   :members:

.. doxygenstruct:: pti_pci_properties_t
   :members:

.. doxygenstruct:: pti_metric_properties_t
   :members:

.. doxygenstruct:: pti_metrics_group_properties_t
   :members:

.. doxygenstruct:: pti_metrics_group_collection_params_t
   :members:

Enumerators
-----------

.. doxygenenum:: pti_metrics_group_type

.. doxygenenum:: pti_metric_type

.. doxygenenum:: pti_metric_value_type

Type Definitions
----------------

.. doxygentypedef:: pti_device_handle_t

.. doxygentypedef:: pti_metrics_group_handle_t
