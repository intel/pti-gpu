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
find_library(
  unified-runtime_LIBRARY
  NAMES ur_loader
  HINTS ENV LIBRARY_PATH
        ENV LD_LIBRARY_PATH
        ENV LIB
        ENV CMPLR_ROOT
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux
        /opt/sycl
  PATH_SUFFIXES lib
                linux/lib
)

if(unified-runtime_LIBRARY)
  # Get the root directory of the unified-runtime installation. This will help
  # us find the include directory, given some versions of the compiler do not
  # have an environment variable pointing to the installation root.
  get_filename_component(UNIFIED_RUNTIME_LIB_DIR "${unified-runtime_LIBRARY}" DIRECTORY)
  get_filename_component(UNIFIED_RUNTIME_ROOT_DIR "${UNIFIED_RUNTIME_LIB_DIR}" DIRECTORY)
endif()

find_path(
  unified-runtime_INCLUDE_DIR
# the file order seems impacts if it finds UR in the dir under CMPLR_ROOT
  NAMES ur_api.h sycl/ur_api.h
  HINTS ${UNIFIED_RUNTIME_ROOT_DIR}
        ENV CPATH
        ENV C_INCLUDE_PATH
        ENV CPLUS_INCLUDE_PATH
        ENV INCLUDE
        ENV CMPLR_ROOT
  PATHS /opt/intel/oneapi/compiler/latest
        /opt/intel/oneapi/compiler/latest/linux
        /opt/sycl
  PATH_SUFFIXES include
                linux/include
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
