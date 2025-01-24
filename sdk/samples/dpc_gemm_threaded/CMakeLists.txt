cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_DPC_GEMM_THREADED CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requres a sycl compatible compiler")
  return()
endif()

add_executable(dpc_gemm_threaded main.cc)

target_compile_options(dpc_gemm_threaded PRIVATE -fsycl -gline-tables-only)
target_link_options(dpc_gemm_threaded PRIVATE -fsycl -gline-tables-only)
target_include_directories(dpc_gemm_threaded PRIVATE
                           "${PROJECT_SOURCE_DIR}/../samples_utilities")

if (NOT TARGET Threads::Threads)
  find_package(Threads REQUIRED)
endif()

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(dpc_gemm_threaded PUBLIC Threads::Threads Pti::pti_view)
