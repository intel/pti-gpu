# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindOpenCLTracing
-------

`Compute Runtime <https://github.com/intel/compute-runtime/tree/master/opencl/source/tracing>`_

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``OpenCL::TracingHeaders``

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``OpenCLTracing_FOUND``
  True if the system has the OpenCLTracing library.
``OpenCLTracing_INCLUDE_DIRS``
  Include directories needed to use OpenCLTracing.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``OpenCLTracing_INCLUDE_DIR``
  The directory containing ``CL/tracing_api.h``.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html
find_path(
  OpenCLTracing_INCLUDE_DIR
  NAMES CL/tracing_api.h
  HINTS ENV CPATH
        ENV INTELOCLSDKROOT
        ENV INCLUDE
        ${OpenCL_INCLUDE_DIR}
  PATH_SUFFIXES include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  OpenCLTracing
  FOUND_VAR OpenCLTracing_FOUND
  REQUIRED_VARS OpenCLTracing_INCLUDE_DIR)

if(OpenCLTracing_FOUND AND NOT TARGET OpenCL::TracingHeaders)
  add_library(OpenCL::TracingHeaders INTERFACE IMPORTED)
  set_target_properties(
    OpenCL::TracingHeaders PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${OpenCLTracing_INCLUDE_DIR}")
endif()

mark_as_advanced(OpenCLTracing_INCLUDE_DIR)
