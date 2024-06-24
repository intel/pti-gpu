# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindDevUtilities
-------

Finds the dpc_common library. This is a library that is part of the standard
oneAPI installation but is not part of a oneAPI component.

It is a header-only library. This module just finds its include path.

It is primarily used in the
`oneAPI samples <https://github.com/oneapi-src/oneAPI-samples>`_

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``DevUtilities::utils``
  Common samples include path

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``DevUtilities_FOUND``
  True if the system has the DevUtilities library.
``DevUtilities_INCLUDE_DIRS``
  Include directories needed to use DevUtilities.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``DevUtilities_INCLUDE_DIR``
  The directory containing ``dpc_common.hpp``.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html
find_path(
  DevUtilities_INCLUDE_DIR
  NAMES dpc_common.hpp
  HINTS ENV CPATH
	ENV LD_LIBRARY_PATH
	ENV CMPLR_ROOT
  PATHS /opt/intel/oneapi/dev-utilities
        /opt/intel/oneapi/dev-utilities/latest
        /opt/intel/oneapi/dev-utilities/linux
	"C:/Program Files (x86)/Intel/oneAPI/dev-utilities/latest"
  PATH_SUFFIXES include
                linux/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  DevUtilities
  FOUND_VAR DevUtilities_FOUND
  REQUIRED_VARS DevUtilities_INCLUDE_DIR)

if(DevUtilities_FOUND AND NOT TARGET DevUtilities::utils)
  add_library(DevUtilities::utils INTERFACE IMPORTED)
  set_target_properties(
    DevUtilities::utils PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${DevUtilities_INCLUDE_DIR}")
endif()

mark_as_advanced(DevUtilities_INCLUDE_DIR)
