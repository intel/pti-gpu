==============
Known Issues
==============

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

1. Metrics collection (``ptiMetrics`` and ``ptiMetricsScope`` APIs) requires metrics to be enabled on the device, either by calling ``ptiMetricsEnable`` or by setting the ``ZET_ENABLE_METRICS=1`` environment variable. Drivers that do not support enabling metrics at runtime make ``ptiMetricsEnable`` return ``PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED``; on those drivers the environment variable is still required.

2. Communication tracing (``PTI_VIEW_COMMUNICATION``) automatically registers ``libpti_view.so`` through ``INTEL_LIBITTNOTIFY64`` when the variable is unset. If it already identifies the loaded ``libpti_view.so``, PTI communication tracing remains enabled; if it identifies another collector, PTI leaves it unchanged and disables PTI oneCCL tracing. Because ``libpti_view.so`` self-registers whenever it is loaded, applications that link it unconditionally cause ITT to be loaded even when communication tracing is disabled. oneCCL tracing remains controlled through the normal PTI view enable/disable mechanisms, and PTI limits additional overhead by disabling non-oneCCL ITT domains during collector initialization.

3. Significant CPU overhead is currently observed when tracing workloads that frequently submit short kernels to the GPU. Optimization efforts continue both in the library as well as in the underlying layers of the software stack.

4. For ``on-demand`` collection mode, Rolling drivers are required. Older LTS drivers do not support it, and instead require Level Zero tracing to be globally enabled via the ``ZE_ENABLE_TRACING_LAYER=1`` environment variable.

5. Kernel name demangling is not supported on Windows.
