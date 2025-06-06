cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_ONEMKL_GEMM CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requres a sycl compatible compiler")
  return()
endif()

if (NOT TARGET MKL::MKL)
  find_package(MKL CONFIG PATHS $ENV{MKLROOT} NO_DEFAULT_PATH)
  message(STATUS "Imported oneMKL targets: ${MKL_IMPORTED_TARGETS}")
endif()

if (NOT TARGET MKL::MKL)
  message(WARNING "${PROJECT_NAME} requires oneMKL installed")
  return()
endif()

add_executable(onemkl_gemm_exe onemkl_gemm.cc)

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(onemkl_gemm_exe PUBLIC MKL::MKL MKL::MKL_SYCL Pti::pti_view)
target_include_directories(onemkl_gemm_exe
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils"
                           "${PROJECT_SOURCE_DIR}/../samples_utilities")
