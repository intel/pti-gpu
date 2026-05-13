==============
Known Issues
==============

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

1. Metrics collection (``ptiMetrics`` and ``ptiMetricsScope`` APIs) requires the ``ZET_ENABLE_METRICS=1`` environment variable to be set.

2. Communication tracing (``PTI_VIEW_COMMUNICATION``) requires the ``INTEL_LIBITTNOTIFY64`` environment variable to be set, pointing to the ``pti_view`` library path. Without this, communication tracing will not be enabled.

3. Significant CPU overhead is currently observed when tracing workloads that frequently submit short kernels to the GPU. Optimization efforts continue both in the library as well as in the underlying layers of the software stack.

4. For ``on-demand`` collection mode, Rolling drivers are required. Older LTS drivers do not support it, and instead require Level Zero tracing to be globally enabled via the ``ZE_ENABLE_TRACING_LAYER=1`` environment variable.

5. Kernel name demangling is not supported on Windows.
