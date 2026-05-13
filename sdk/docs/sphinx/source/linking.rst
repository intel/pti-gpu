=========
Linking
=========

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

Use CMake ``find_package``

.. code-block:: cmake

   # Set Pti_DIR if you install in a nonstandard location.
   set(Pti_DIR <path-to-pti>/lib/cmake/pti)

   # Find the PTI package (version is optional)
   find_package(Pti REQUIRED)

   # Link against PTI library
   target_link_libraries(your_target PUBLIC Pti::pti_view)

Available CMake Targets
-----------------------

After ``find_package(Pti)``, the following targets are available:

* ``Pti::pti_view`` - Main PTI View library for tracing and profiling
* ``Pti::pti`` - PTI core library (module library)
* ``Pti::pti_all`` - Interface target including all PTI components

For most applications, use ``Pti::pti_view``.
