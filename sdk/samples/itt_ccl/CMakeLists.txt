#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
cmake_minimum_required(VERSION 3.16)
project(PTI_Samples_ITT_CCL CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(WARNING "${PROJECT_NAME} is only supported in Linux")
  return()
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if (NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler")
  return()
endif()

find_package(oneCCL)

if (NOT TARGET oneCCL)
  message(WARNING "${PROJECT_NAME} requires a oneCCL installed.")
  return()
endif()

if (NOT TARGET Pti::pti_view)
  find_package(Pti)
endif()

add_executable(sycl_allreduce_usm_test sycl_allreduce_usm_test.cpp)

target_link_options(sycl_allreduce_usm_test PUBLIC -fsycl)
target_compile_options(sycl_allreduce_usm_test PUBLIC -fsycl)

target_include_directories(sycl_allreduce_usm_test PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities"
                                                           "${PROJECT_SOURCE_DIR}/../../src/utils")

target_compile_definitions(
  sycl_allreduce_usm_test
  PRIVATE $<$<TARGET_EXISTS:Pti::pti_view>:USE_PTI_VIEW>
)

get_target_property(oneCCL_PATH oneCCL INTERFACE_INCLUDE_DIRECTORIES)
get_filename_component(oneCCL_ROOT ${oneCCL_PATH} DIRECTORY)

target_link_libraries(sycl_allreduce_usm_test PUBLIC oneCCL $<TARGET_NAME_IF_EXISTS:Pti::pti_view> ${FS_LIB})
target_include_directories(sycl_allreduce_usm_test SYSTEM PRIVATE ${oneCCL_ROOT}/share/doc/ccl/examples/include)
