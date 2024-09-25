# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
Findunified-runtime
-------

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``unified-runtime::loader``
  ur_loader.so

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``unified-runtime_FOUND``
  True if the system has the Ur library.
``unified-runtime_INCLUDE_DIRS``
  Include directories needed to use Ur.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``unified-runtime_INCLUDE_DIR``
  The directory containing ``sycl/ur_api.h``.
``unified-runtime_LIBRARY``
The path to the Unified Runtime library.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html

#
# Note about PATHS:
#
# These are specifically hardcoded fallbacks.
#
# * `/opt/intel/oneapi/compiler/latest` - common oneAPI installation location on
#                                         Linux.
# * `/opt/sycl` - Intel/LLVM open-source compiler container installation location
#                 on Linux.
#
find_path(
  unified-runtime_INCLUDE_DIR
  NAMES sycl/ur_api.h
  HINTS ENV CMPLR_ROOT
        ENV CPATH
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux
        /opt/sycl
  PATH_SUFFIXES include
                linux/include
)

find_library(
  unified-runtime_LIBRARY
  NAMES ur_loader
  HINTS ENV CMPLR_ROOT
        ENV LIBRARY_PATH
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux
        /opt/sycl
  PATH_SUFFIXES lib
                linux/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  unified-runtime
  FOUND_VAR unified-runtime_FOUND
  REQUIRED_VARS unified-runtime_LIBRARY unified-runtime_INCLUDE_DIR)

if(unified-runtime_FOUND AND NOT TARGET unified-runtime::loader)
  add_library(unified-runtime::loader UNKNOWN IMPORTED)
  set_target_properties(
    unified-runtime::loader
    PROPERTIES IMPORTED_LOCATION "${unified-runtime_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${unified-runtime_INCLUDE_DIR}")
endif()

mark_as_advanced(unified-runtime_INCLUDE_DIR unified-runtime_LIBRARY)
