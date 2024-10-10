# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindXpti
-------

Finds the Xpti library. Xpti is part of the Intel/LLVM project. It is also
included as part of the standard oneAPI installation.

`Intel/LLVM <https://github.com/intel/llvm>`_

Further documentation on linking and using XPTI can be found here:
https://github.com/intel/llvm/blob/sycl/xptifw/doc/XPTI_Framework.md.

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
``Xpti_Static_LIBRARY``
  The path to the Xpti static library.
``Xpti_Shared_LIBRARY``
  The path to the Xpti shared library.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html

#
# Note about PATHS:
#
# These are specifically hardcoded fallbacks.
#
# * `/opt/intel/oneapi/compiler/latest` - common oneAPI installation location on
#   Linux.
# * `/opt/sycl` - Intel/LLVM open-source compiler container installation
#   location on Linux.
#

find_path(
  Xpti_INCLUDE_DIR
  NAMES xpti/xpti_trace_framework.h
  HINTS ENV CMPLR_ROOT ENV CPATH
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux /opt/sycl
  PATH_SUFFIXES include linux/include)

find_library(
  Xpti_Static_LIBRARY
  NAMES xpti
  HINTS ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux /opt/sycl
  PATH_SUFFIXES lib linux/lib)

find_library(
  Xpti_Shared_LIBRARY
  NAMES xptifw
  HINTS ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux /opt/sycl
  PATH_SUFFIXES lib linux/lib)

if(WIN32)
find_library(
  Xpti_Static_Debug_LIBRARY
  NAMES xptid
  HINTS ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATH_SUFFIXES lib linux/lib)

find_library(
  Xpti_Shared_Debug_LIBRARY
  NAMES xptifwd
  HINTS ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATH_SUFFIXES lib linux/lib)

find_file(
  Xpti_Shared_Runtime_LIBRARY
  NAMES xptifw.dll
  HINTS ENV PATH ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATH_SUFFIXES bin windows/bin)

find_file(
  Xpti_Shared_Debug_Runtime_LIBRARY
  NAMES xptifwd.dll
  HINTS ENV PATH ENV CMPLR_ROOT ENV LIBRARY_PATH
  PATH_SUFFIXES bin windows/bin)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Xpti
  FOUND_VAR Xpti_FOUND
  REQUIRED_VARS Xpti_Shared_LIBRARY Xpti_INCLUDE_DIR)

if(Xpti_FOUND AND NOT TARGET Xpti::xpti)
  add_library(Xpti::xpti SHARED IMPORTED)
  set_target_properties(
    Xpti::xpti
    PROPERTIES IMPORTED_LOCATION "${Xpti_Shared_LIBRARY}"
               INTERFACE_COMPILE_DEFINITIONS
               "XPTI_API_EXPORTS;XPTI_CALLBACK_API_EXPORTS"
               INTERFACE_INCLUDE_DIRECTORIES "${Xpti_INCLUDE_DIR}")
  if(WIN32)
    set_target_properties(
      Xpti::xpti
      PROPERTIES RUNTIME_OUTPUT_NAME "xptifw.dll"
                 RUNTIME_OUTPUT_NAME_RELEASE "xptifw.dll"
                 RUNTIME_OUTPUT_NAME_DEBUG "xptifwd.dll")
  endif()

  if(UNIX)
    set_target_properties(
      Xpti::xpti
      PROPERTIES RUNTIME_OUTPUT_NAME "libxptifw.so"
                 RUNTIME_OUTPUT_NAME_RELEASE "libxptifw.so"
                 RUNTIME_OUTPUT_NAME_DEBUG "libxptifw.so")
  endif()

  # Just a guess, not actually supported.
  if(APPLE)
    set_target_properties(
      Xpti::xpti
      PROPERTIES RUNTIME_OUTPUT_NAME "libxptifw.dylib"
                 RUNTIME_OUTPUT_NAME_RELEASE "libxptifw.dylib"
                 RUNTIME_OUTPUT_NAME_DEBUG "libxptifw.dylib")
  endif()

  if(WIN32)
    # Since XPTI on Windows supports multiple configurations (Debug and Release)
    # we should handle both those cases.
    # Note the difference between IMPORTED_IMPLIB and IMPORTED_LOCATION. On
    # Windows, IMPORTED_IMPLIB should be the .lib file and IMPORTED_LOCATION
    # should be the .dll file for SHARED libraries.
    if(Xpti_Shared_LIBRARY)
      set_property(
        TARGET Xpti::xpti
        APPEND
        PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(Xpti::xpti PROPERTIES IMPORTED_IMPLIB_RELEASE
                                                  "${Xpti_Shared_LIBRARY}")
      if(Xpti_Shared_Runtime_LIBRARY)
        set_target_properties(
          Xpti::xpti
          PROPERTIES IMPORTED_LOCATION "${Xpti_Shared_Runtime_LIBRARY}"
                     IMPORTED_LOCATION_RELEASE "${Xpti_Shared_Runtime_LIBRARY}")
      endif()
    endif()

    if(Xpti_Shared_Debug_LIBRARY)
      set_property(
        TARGET Xpti::xpti
        APPEND
        PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(
        Xpti::xpti PROPERTIES IMPORTED_IMPLIB_DEBUG
                              "${Xpti_Shared_Debug_LIBRARY}")
      if(Xpti_Shared_Debug_Runtime_LIBRARY)
        set_target_properties(
          Xpti::xpti PROPERTIES IMPORTED_LOCATION_DEBUG
                                "${Xpti_Shared_Debug_Runtime_LIBRARY}")
      endif()
    endif()
  endif()
endif()

if(Xpti_FOUND AND NOT TARGET Xpti::xpti_static)
  add_library(Xpti::xpti_static STATIC IMPORTED)
  set_target_properties(
    Xpti::xpti_static
    PROPERTIES IMPORTED_LOCATION "${Xpti_Static_LIBRARY}"
               INTERFACE_COMPILE_DEFINITIONS "XPTI_STATIC_LIBRARY"
               INTERFACE_INCLUDE_DIRECTORIES "${Xpti_INCLUDE_DIR}")
  if(WIN32)
    if(Xpti_Static_LIBRARY)
      set_property(
        TARGET Xpti::xpti_static
        APPEND
        PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(
        Xpti::xpti_static
        PROPERTIES IMPORTED_IMPLIB_RELEASE "${Xpti_Static_LIBRARY}"
                   IMPORTED_LOCATION "${Xpti_Static_LIBRARY}"
                   IMPORTED_LOCATION_RELEASE "${Xpti_Static_LIBRARY}")
    endif()

    if(Xpti_Static_Debug_LIBRARY)
      set_property(
        TARGET Xpti::xpti_static
        APPEND
        PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(
        Xpti::xpti_static
        PROPERTIES IMPORTED_IMPLIB_DEBUG "${Xpti_Static_Debug_LIBRARY}"
                   IMPORTED_LOCATION_DEBUG "${Xpti_Static_Debug_LIBRARY}")
    endif()
  endif()
endif()

mark_as_advanced(Xpti_INCLUDE_DIR Xpti_Static_LIBRARY Xpti_Shared_LIBRARY)
