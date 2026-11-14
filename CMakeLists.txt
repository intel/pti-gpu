# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# Aggregate build of the PTI-GPU tools and samples.
#
# Every tool and sample is a standalone CMake project (see build_utils/), so
# each one is built here as an ExternalProject in its own binary directory:
#   ${CMAKE_BINARY_DIR}/tools/<name>
#   ${CMAKE_BINARY_DIR}/samples/<name>
# The per-folder builds used by tests/run.py keep working unchanged.
#
# Options:
#   PTI_GPU_BUILD_TOOLS    Build the tools (default ON)
#   PTI_GPU_BUILD_SAMPLES  Build the samples (default ON)
#   PTI_GPU_TOOLS          List of tools to build (default: all)
#   PTI_GPU_SAMPLES        List of samples to build (default: all)
#   PTI_GPU_SKIP           List of tools/samples to skip, e.g. "instcount;memaccess"
#   BUILD_TESTING          Add the "legacy" ctest that runs tests/run.py
cmake_minimum_required(VERSION 3.22)

include(cmake/helpers.cmake)

SetPtiGpuVersion()

project(
  pti-gpu
  VERSION ${PTI_GPU_VERSION}
  DESCRIPTION "Profiling and tracing tools and samples for Intel(R) GPUs"
  HOMEPAGE_URL "https://github.com/vladimir-tsymbal/pti-gpu"
  LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The build_utils Windows paths are kept, but this fork develops and
  # tests on Linux only.
  message(WARNING "Only Linux is tested in this fork; ${CMAKE_SYSTEM_NAME} builds may not work")
endif()

include(ExternalProject)

set(PTI_GPU_ALL_TOOLS
    cl_tracer
    ze_tracer
    onetrace
    oneprof
    gpuinfo
    instcount
    memaccess
    sysmon)

set(PTI_GPU_ALL_SAMPLES
    cl_debug_info
    cl_gemm
    cl_gemm_inst
    cl_gemm_itt
    cl_gpu_metrics
    cl_gpu_query
    cl_hot_functions
    cl_hot_kernels
    dpc_gemm
    dpc_info
    gpu_perfmon_set
    omp_gemm
    omp_hot_regions
    ze_debug_info
    ze_gemm
    ze_hot_functions
    ze_hot_kernels
    ze_info
    ze_metric_info
    ze_metric_query
    ze_metric_streamer
    ze_sysman)

# Samples that hard-code icpx as their compiler.
set(PTI_GPU_ICPX_SAMPLES dpc_gemm dpc_info omp_gemm omp_hot_regions)
# Samples that need the IGA library (FindIGALibrary is fatal without it).
set(PTI_GPU_IGA_SAMPLES cl_debug_info ze_debug_info)

option(PTI_GPU_BUILD_TOOLS "Build the PTI-GPU tools" ON)
option(PTI_GPU_BUILD_SAMPLES "Build the PTI-GPU samples" ON)
set(PTI_GPU_TOOLS
    "${PTI_GPU_ALL_TOOLS}"
    CACHE STRING "Tools to build")
set(PTI_GPU_SAMPLES
    "${PTI_GPU_ALL_SAMPLES}"
    CACHE STRING "Samples to build")
set(PTI_GPU_SKIP
    ""
    CACHE STRING "Tools and samples to skip")
option(BUILD_TESTING "Build tests" OFF)

include(CTest)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

# Cheap dependency probes. A missing dependency skips the affected components
# with a message instead of failing the whole build. Linux only: on Windows
# the samples pick other compilers and build_utils locates IGA by its DLL.
if(WIN32)
  set(PTI_GPU_ICPX ON)
  set(PTI_GPU_IGA_LIB ON)
else()
  find_program(PTI_GPU_ICPX icpx)
  find_library(PTI_GPU_IGA_LIB NAMES iga64)
endif()

# Settings forwarded to each sub-project when they are set here.
set(PTI_GPU_EP_CMAKE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
foreach(_var CMAKE_PREFIX_PATH CMAKE_INCLUDE_PATH CMAKE_LIBRARY_PATH GTPIN_PATH)
  if(DEFINED ${_var} AND NOT "${${_var}}" STREQUAL "")
    string(REPLACE ";" "|" _val "${${_var}}")
    list(APPEND PTI_GPU_EP_CMAKE_ARGS "-D${_var}=${_val}")
  endif()
endforeach()

# Returns TRUE in OUT_VAR if component NAME should be built.
function(pti_gpu_component_enabled NAME OUT_VAR)
  set(_enabled TRUE)
  if(NAME IN_LIST PTI_GPU_SKIP)
    message(STATUS "Skipping ${NAME}: listed in PTI_GPU_SKIP")
    set(_enabled FALSE)
  elseif(NAME IN_LIST PTI_GPU_ICPX_SAMPLES AND NOT PTI_GPU_ICPX)
    message(STATUS "Skipping ${NAME}: icpx not found (source oneAPI setvars.sh)")
    set(_enabled FALSE)
  elseif(NAME IN_LIST PTI_GPU_IGA_SAMPLES AND NOT PTI_GPU_IGA_LIB)
    message(STATUS "Skipping ${NAME}: IGA library (libiga64) not found")
    set(_enabled FALSE)
  endif()
  set(${OUT_VAR}
      ${_enabled}
      PARENT_SCOPE)
endfunction()

set(PTI_GPU_ENABLED_TOOLS "")
set(PTI_GPU_STAGE_DIR "${CMAKE_BINARY_DIR}/stage")

if(PTI_GPU_BUILD_TOOLS)
  foreach(_tool IN LISTS PTI_GPU_TOOLS)
    if(NOT _tool IN_LIST PTI_GPU_ALL_TOOLS)
      message(FATAL_ERROR "Unknown tool in PTI_GPU_TOOLS: ${_tool}")
    endif()
    pti_gpu_component_enabled(${_tool} _enabled)
    if(NOT _enabled)
      continue()
    endif()
    # Each tool installs into its own staging prefix; the top-level install()
    # below copies it into CMAKE_INSTALL_PREFIX as a per-tool CPack component.
    ExternalProject_Add(
      ${_tool}
      SOURCE_DIR "${PROJECT_SOURCE_DIR}/tools/${_tool}"
      BINARY_DIR "${CMAKE_BINARY_DIR}/tools/${_tool}"
      INSTALL_DIR "${PTI_GPU_STAGE_DIR}/${_tool}"
      LIST_SEPARATOR "|"
      CMAKE_ARGS ${PTI_GPU_EP_CMAKE_ARGS} "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
      BUILD_ALWAYS ON)
    install(
      DIRECTORY "${PTI_GPU_STAGE_DIR}/${_tool}/"
      DESTINATION .
      USE_SOURCE_PERMISSIONS
      COMPONENT ${_tool})
    list(APPEND PTI_GPU_ENABLED_TOOLS ${_tool})
  endforeach()
endif()

if(PTI_GPU_BUILD_SAMPLES)
  foreach(_sample IN LISTS PTI_GPU_SAMPLES)
    if(NOT _sample IN_LIST PTI_GPU_ALL_SAMPLES)
      message(FATAL_ERROR "Unknown sample in PTI_GPU_SAMPLES: ${_sample}")
    endif()
    pti_gpu_component_enabled(${_sample} _enabled)
    if(NOT _enabled)
      continue()
    endif()
    ExternalProject_Add(
      ${_sample}
      SOURCE_DIR "${PROJECT_SOURCE_DIR}/samples/${_sample}"
      BINARY_DIR "${CMAKE_BINARY_DIR}/samples/${_sample}"
      LIST_SEPARATOR "|"
      CMAKE_ARGS ${PTI_GPU_EP_CMAKE_ARGS}
      BUILD_ALWAYS ON
      INSTALL_COMMAND "")
  endforeach()
endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  # Builds and runs every sample and tool in-tree (<dir>/build), as before.
  add_test(NAME legacy_run_py
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run.py
           WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests)
  set_tests_properties(legacy_run_py PROPERTIES LABELS legacy)
endif()

include(cmake/packaging.cmake)
