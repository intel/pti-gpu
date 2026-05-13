============================
Building and Installing
============================

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Using Pre-built Binaries
=======================================

The easiest way to use PTI SDK is to install the pre-built package from PyPI:

.. code-block:: bash

   pip install intel-pti

After installation, the PTI libraries will be available in your Python environment.


Building from Source
====================

If you need to build PTI SDK from source (for development or custom configurations), follow these instructions.

Prerequisites
-------------

* **CMake**: Version 3.14 or higher
* **Git**: Version 1.8 or higher
* **Intel(R) oneAPI Base Toolkit**: 2025.1.0 or higher (2026.0 tested)
* **Level Zero Loader**: Version 1.28.0 or higher
* **C++17 compatible compiler**: GCC, Intel C++ Compiler (icpx)

**Platform-Specific:**

* **Linux**: GCC or Intel C++ Compiler (icpx)
* **Windows**: Visual Studio 2022 or higher, Intel C++ Compiler (icpx), Ninja build system

See `INSTALL.md <https://github.com/intel/pti-gpu/blob/master/sdk/INSTALL.md>`_ for detailed prerequisites and tested configurations.

Environment Setup
-----------------

**Linux:**

.. code-block:: bash

   source <path_to_oneapi>/setvars.sh
   cd sdk

**Windows:**

.. code-block:: batch

   call "C:\Program Files (x86)\Intel\oneAPI\setvars-vcvarsall.bat"
   call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
   cd sdk

Configure and Build
-------------------

**Configure (Debug):**

.. code-block:: bash

   CC=icx CXX=icpx cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

**Configure (Release):**

.. code-block:: bash

   CC=icx CXX=icpx cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

.. note::
   You can build with GCC by omitting the ``CC=icx CXX=icpx`` prefix.

**Build:**

.. code-block:: bash

   cmake --build build --parallel 4

Running Tests
=============

After building, verify the build by running the test suite:

.. code-block:: bash

   ctest --test-dir build --output-on-failure

**Running Sample Applications:**

**Linux:**

.. code-block:: bash

   ./build/bin/vec_sqadd

**Windows:**

.. code-block:: batch

   build\bin\vec_sqadd.exe

If the sample runs successfully and produces output, your build is working correctly.

Installation (Advanced)
=======================

Most users do not need to install PTI SDK. Installation is only necessary if you want to:

* Install PTI SDK system-wide or to a custom location
* Use PTI SDK with CMake ``find_package()`` in other projects
* Package PTI SDK for distribution

From the build directory:

.. code-block:: bash

   cmake --install build --config Release --prefix "../out"

You can specify a different installation prefix:

**Linux:**

.. code-block:: bash

   cmake --install build --config Release --prefix "/usr/local"

**Windows:**

.. code-block:: batch

   cmake --install build --config Release --prefix "C:\Program Files\PTI"

The installation includes:

* Header files in ``<prefix>/include/pti/``
* Library files in ``<prefix>/lib/``
* CMake configuration files in ``<prefix>/lib/cmake/pti/``

Build Options
=============

Common CMake Options
--------------------

* ``-DCMAKE_BUILD_TYPE=<Debug|Release>`` - Set build type
* ``-DCMAKE_CXX_STANDARD=<17|20>`` - Set C++ standard (minimum 17)
* ``-DCMAKE_CXX_COMPILER=<compiler>`` - Set C++ compiler (e.g., g++, icpx)
* ``-DCMAKE_C_COMPILER=<compiler>`` - Set C compiler (e.g., gcc, icx)
* ``-DBUILD_TESTING=<ON|OFF>`` - Enable/disable building tests (default: ON)

PTI Custom Options
------------------

* ``-DPTI_BUILD_SAMPLES=<ON|OFF>`` - Enable/disable building samples (default: ON when building as top-level project)
* ``-DPTI_BUILD_TESTING=<ON|OFF>`` - Enable/disable building PTI tests (default: ON when building as top-level project)
* ``-DPTI_INSTALL=<ON|OFF>`` - Enable/disable library installation when invoking ``--install`` (default: ON)
* ``-DPTI_ENABLE_LOGGING=<ON|OFF>`` - Enable internal logging for PTI (default: OFF)

**Example - Build library only without tests or samples:**

.. code-block:: bash

   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_TESTING=OFF \
         -DPTI_BUILD_SAMPLES=OFF

Troubleshooting
===============

CMake cannot find oneAPI components
------------------------------------

Ensure you've sourced the oneAPI environment:

* **Linux**: ``source <path_to_oneapi>/setvars.sh``
* **Windows**: Run ``<path_to_oneapi>\setvars.bat`` in cmd

Level Zero headers not found
-----------------------------

* Verify Level Zero Loader is installed (version >= 1.28.0)
* Ensure oneAPI environment is properly sourced/initialized

Build fails with compiler errors
---------------------------------

* Ensure you're using a supported oneAPI version (2025.1.0 - 2025.3.0, or 2026.0)
* Verify C++17 support is available
* Try cleaning the build directory and reconfiguring

For more detailed instructions and troubleshooting, see `INSTALL.md <https://github.com/intel/pti-gpu/blob/master/sdk/INSTALL.md>`_.
