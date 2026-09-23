# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
cmake_minimum_required(VERSION 3.22)

include(cmake/helpers.cmake)

SetPtiGpuVersion()

project(
  pti-gpu
  VERSION ${PTI_GPU_VERSION}
  DESCRIPTION "Intel Profiling Tools"
  HOMEPAGE_URL "https://github.com/intel/pti-gpu"
  LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
  if(NOT APPLE)
    list(APPEND CMAKE_BUILD_RPATH $ORIGIN)
  endif()
  GetCurrentGitRev(PTI_GPU_COMMIT_HASH)
endif()

option(PTI_GPU_BUILD_SDK "Build PTI" ON)
option(PTI_GPU_BUILD_UNITRACE "Build unitrace" ON)
# TODO(PTI/UNITRACE): Enable Testing from Top-Level CMakeLists.txt.
# For now, one must build from the component subdirectories
# (e.g., sdk or tools/unitrace) to enable testing.
option(BUILD_TESTING "Build tests" OFF)

include(CTest)

if(PTI_GPU_BUILD_SDK)
  if(PTI_GPU_COMMIT_HASH)
    set(PTI_COMMIT_HASH
        ${PTI_GPU_COMMIT_HASH}
        CACHE INTERNAL "PTI commit hash")
  endif()
  set(PTI_BUILD_TESTING
      ${BUILD_TESTING}
      CACHE INTERNAL "Build PTI tests")
  set(PTI_BUILD_SAMPLES
      ${BUILD_TESTING}
      CACHE INTERNAL "Build PTI samples")
  add_subdirectory(sdk)
endif()

if(PTI_GPU_BUILD_UNITRACE)
  if(NOT MPI_FOUND)
    find_package(MPI QUIET)
  endif()
  if(NOT MPI_FOUND)
    set(BUILD_WITH_MPI
        OFF
        CACHE BOOL "Build Unitrace with MPI support")
  endif()
  add_subdirectory(tools/unitrace)
endif()

if(PROJECT_IS_TOP_LEVEL)
  include(cmake/packaging.cmake)
endif()
