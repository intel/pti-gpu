=====================
System Requirements
=====================

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Supported Hardware
------------------

PTI SDK supports the following Intel GPUs:

**Intel(R) Data Center GPUs:**

* Intel(R) Data Center GPU Max Series (Max 1550, Max 1100)
* Intel(R) Data Center GPU Flex Series

**Intel(R) Client GPUs:**

* Intel(R) Arc(TM) Graphics (A-Series and B-Series)

Prerequisites
---------------

* `Intel(R) oneAPI Base Toolkit <https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html>`_ (:ref:`versions <oneAPITestedVersions>`)
* `Level Zero Loader <https://github.com/oneapi-src/level-zero>`_ (version >= 1.14.0 at runtime; building requires headers >= 1.32.0, which CMake downloads if they are not found)

.. note::
   For the best experience, use a recent Rolling GPU driver and Level Zero loader (for reference, PTI 1.2.0 is validated with Level Zero loader 1.32.0 and Intel(R) compute-runtime 26.31.39395.13). Graph profiling and on-demand collection require Rolling drivers.

Intel(R) oneAPI Base Toolkit Tested Versions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _oneAPITestedVersions:

.. list-table::
   :widths: 25 25 25
   :header-rows: 1

   * - Support
     - Linux
     - Windows
   * - Full
     - 2026.0, 2026.1
     - 2026.0

OS Specific
^^^^^^^^^^^^^
.. list-table::
   :widths: 25 25 25
   :header-rows: 1

   * -
     - Linux
     - Windows
   * - `CMake <https://cmake.org/>`_
     - version 3.14 and above. For presets, >= 3.20
     - version >= 3.23
   * - `Ninja <https://github.com/ninja-build/ninja>`_
     - version >= 1.10.1 (optional)
     - version 1.12.1
   * - `Git <https://git-scm.com/>`_
     - version 1.8 and above
     - N/A
   * - `Visual Studio 2022 <https://visualstudio.microsoft.com/vs/>`_
     - N/A
     - version >= 17.10.3

Supported Operating Systems
-----------------------------

* Ubuntu 22.04, 24.04
* Rocky 8, Rocky 9
* RHEL 8, RHEL 9
* SLES 15
* Windows 11

Regularly Tested Configurations
---------------------------------

- Ubuntu 22.04 with Intel(R) Data Center GPU Max 1550
- Ubuntu 22.04 with Intel(R) Data Center GPU Max 1100
- Ubuntu 24.04 with Intel(R) Data Center GPU Max Series
- Ubuntu 25.10 with Intel(R) Arc(TM) Graphics (A-Series and B-Series)
- Rocky 9 with Intel(R) Data Center GPU Max Series
- Windows 11 with Intel(R) Arc(TM) Graphics (A-Series and B-Series)
- Windows 11 with Intel(R) Data Center GPU Flex Series



