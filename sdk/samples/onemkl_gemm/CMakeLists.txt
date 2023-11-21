set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Modules/")
include(macros)

setrequiredcmakeversion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_KERNEL_V CXX)
setcompilerflags()
setbuildtype()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(FATAL_ERROR "${PROJECT_NAME} requres a sycl compatible compiler")
endif()

add_executable(onemkl_gemm_exe onemkl_gemm.cpp)

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

if (NOT TARGET MKL::MKL)
  find_package(MKL REQUIRED)
endif()

target_compile_options(onemkl_gemm_exe PUBLIC "-fsycl" "-mkl" "-gline-tables-only")
target_link_options(onemkl_gemm_exe PUBLIC "-fsycl" "-mkl" "-gline-tables-only")

target_link_libraries(onemkl_gemm_exe PUBLIC mkl_intel_lp64 mkl_sequential mkl_core mkl_sycl Pti::pti_view)
target_include_directories(onemkl_gemm_exe SYSTEM PUBLIC $<TARGET_PROPERTY:MKL::MKL,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(onemkl_gemm_exe
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils"
                           "${PROJECT_SOURCE_DIR}/../samples_utilities")
