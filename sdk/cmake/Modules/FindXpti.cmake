# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindXpti
-------

Finds the Xpti library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Xpti::xpti``
  XPTI shared
``Xpti::xpti_static``
  XPTI static

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Xpti_FOUND``
  True if the system has the Xpti library.
``Xpti_INCLUDE_DIRS``
  Include directories needed to use Xpti.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Xpti_INCLUDE_DIR``
  The directory containing ``xpti_trace_framework.h``.
``Xpti_STATIC_LIBRARY``
  The path to the Xpti static library.
``Xpti_SHARED_LIBRARY``
  The path to the Xpti shared library.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html
# TODO: Change the copyright?
if(UNIX)
  find_path(
    Xpti_INCLUDE_DIR
    NAMES xpti/xpti_trace_framework.h
    HINTS ENV LD_LIBRARY_PATH
          ENV CMPLR_ROOT
    PATHS /opt/intel/oneapi/compiler/latest
          /opt/intel/oneapi/compiler/latest/linux
    PATH_SUFFIXES include
                  linux/include
  )
  find_library(
    Xpti_STATIC_LIBRARY
    NAMES xpti
    HINTS ENV LD_LIBRARY_PATH
          ENV CMPLR_ROOT
    PATHS /opt/intel/oneapi/compiler/latest
          /opt/intel/oneapi/compiler/latest/linux
    PATH_SUFFIXES lib
                  linux/lib
  )
  find_library(
    Xpti_SHARED_LIBRARY
    NAMES xptifw
    HINTS ENV LD_LIBRARY_PATH
          ENV CMPLR_ROOT
    PATHS /opt/intel/oneapi/compiler/latest
          /opt/intel/oneapi/compiler/latest/linux
    PATH_SUFFIXES lib
                  linux/lib
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Xpti
  FOUND_VAR Xpti_FOUND
  REQUIRED_VARS Xpti_STATIC_LIBRARY Xpti_INCLUDE_DIR)

if(Xpti_FOUND AND NOT TARGET Xpti::xpti)
  add_library(Xpti::xpti UNKNOWN IMPORTED)
  set_target_properties(
    Xpti::xpti
    PROPERTIES IMPORTED_LOCATION "${Xpti_SHARED_LIBRARY}"
               INTERFACE_COMPILE_DEFINITIONS
               "XPTI_API_EXPORTS;XPTI_CALLBACK_API_EXPORTS"
               INTERFACE_INCLUDE_DIRECTORIES "${Xpti_INCLUDE_DIR}")
endif()

if(Xpti_FOUND AND NOT TARGET Xpti::xpti_static)
  add_library(Xpti::xpti_static UNKNOWN IMPORTED)
  set_target_properties(
    Xpti::xpti_static
    PROPERTIES IMPORTED_LOCATION "${Xpti_STATIC_LIBRARY}"
               INTERFACE_COMPILE_DEFINITIONS "XPTI_STATIC_LIBRARY"
               INTERFACE_INCLUDE_DIRECTORIES "${Xpti_INCLUDE_DIR}")
endif()

mark_as_advanced(Xpti_INCLUDE_DIR Xpti_STATIC_LIBRARY Xpti_SHARED_LIBRARY)
