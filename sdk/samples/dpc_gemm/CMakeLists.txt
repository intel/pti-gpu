cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_DPC_GEMM CXX)

# TODO: find_package(IntelSYCL)?
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

add_executable(dpc_gemm main.cc)

target_link_options(dpc_gemm PUBLIC -fsycl -gline-tables-only)
target_compile_options(dpc_gemm PUBLIC -fsycl -gline-tables-only)

target_include_directories(dpc_gemm PRIVATE
  "${PROJECT_SOURCE_DIR}/../../src/utils"
  "${PROJECT_SOURCE_DIR}/../samples_utilities")

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(dpc_gemm PUBLIC Pti::pti_view)
