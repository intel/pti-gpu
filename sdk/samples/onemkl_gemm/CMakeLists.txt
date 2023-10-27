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

add_compile_options(-fsycl -gline-tables-only -mkl -I$MKLROOT/include/)
add_link_options(-fsycl -gline-tables-only -mkl -L$MKLROOT/lib -lmkl_intel_lp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl -lmkl_sycl)

add_executable(onemkl_gemm_exe onemkl_gemm.cpp)
target_include_directories(onemkl_gemm_exe PRIVATE ../samples_utilities)

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(onemkl_gemm_exe PUBLIC Pti::pti_view)

target_include_directories(onemkl_gemm_exe
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(onemkl_gemm_exe PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
