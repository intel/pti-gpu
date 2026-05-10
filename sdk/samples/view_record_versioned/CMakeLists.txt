#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_VIEW_RECORD_VERSIONED CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# TODO: find_package(IntelSYCL)?
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if (NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler")
  return()
endif()

add_executable(view_record_versioned main.cc)

target_link_options(view_record_versioned PUBLIC -fsycl)
target_compile_options(view_record_versioned PUBLIC -fsycl)

target_include_directories(view_record_versioned PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities")

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(view_record_versioned PUBLIC Pti::pti_view)
