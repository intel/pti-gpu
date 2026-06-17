#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_PC_SAMPLING CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler")
  return()
endif()

add_executable(pc_sampling main.cc pc_sampling_client.cc)

target_link_options(pc_sampling PUBLIC -fsycl)
target_compile_options(pc_sampling PUBLIC -fsycl)
target_include_directories(
  pc_sampling PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities"
                          "${PROJECT_SOURCE_DIR}/../../src/utils"
                          )

if (NOT TARGET Threads::Threads)
  find_package(Threads REQUIRED)
endif()

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(pc_sampling PUBLIC spdlog::spdlog Pti::pti_view Threads::Threads)


