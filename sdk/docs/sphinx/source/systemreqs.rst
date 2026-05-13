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
* `Level Zero Loader <https://github.com/oneapi-src/level-zero>`_ (version >= 1.16.15)

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
     - 2025.2, 2025.3, 2026.0
     - 2025.2, 2025.3, 2026.0
   * - Partial
     - 2024.1.x, 2025.1
     - 2024.1.x, 2025.1

OS Specific
^^^^^^^^^^^^^
.. list-table::
   :widths: 25 25 25
   :header-rows: 1

   * -
     - Linux
     - Windows
   * - `CMake <https://cmake.org/>`_
     - version 3.12 and above. For presets, >= 3.20
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



