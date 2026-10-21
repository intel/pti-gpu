#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_PC_SAMPLING_PROFILER CXX)

# LD_PRELOAD injection and the __libc_start_main wrapper are glibc/Linux only.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "${PROJECT_NAME} is supported on Linux only")
  return()
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

if(NOT TARGET LevelZero::level-zero)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Modules")
  find_package(LevelZero REQUIRED)
endif()

add_library(pc_sampling_profiler_collector SHARED collector.cc ../pc_sampling/pc_sampling_client.cc)
target_include_directories(pc_sampling_profiler_collector
                           PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities")
target_link_libraries(pc_sampling_profiler_collector PRIVATE Pti::pti_view ${CMAKE_DL_LIBS})
set_target_properties(pc_sampling_profiler_collector PROPERTIES CXX_VISIBILITY_PRESET hidden)

add_executable(pc_sampling_profiler launcher.cc)
add_dependencies(pc_sampling_profiler pc_sampling_profiler_collector)
target_compile_definitions(
  pc_sampling_profiler
  PRIVATE PC_SAMPLING_COLLECTOR_PATH="$<TARGET_FILE:pc_sampling_profiler_collector>")

if(has_sycl)
  add_executable(pc_sampling_profiler_gemm gemm.cc)
  target_link_options(pc_sampling_profiler_gemm PRIVATE -fsycl)
  target_compile_options(pc_sampling_profiler_gemm PRIVATE -fsycl)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gemm.spv" "${CMAKE_CURRENT_BINARY_DIR}/gemm.spv"
               COPYONLY)
add_executable(pc_sampling_profiler_l0_gemm l0_gemm.cc)
target_link_libraries(pc_sampling_profiler_l0_gemm PRIVATE LevelZero::level-zero)
