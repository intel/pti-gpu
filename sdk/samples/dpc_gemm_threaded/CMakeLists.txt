set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Modules/")
include(macros)

setrequiredcmakeversion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_DPC_GEMM_THREADED CXX)
setcompilerflags()
setbuildtype()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

add_compile_options(-std=c++17 -fsycl -gline-tables-only -pthread)
add_link_options(-fsycl -gline-tables-only -pthread)

add_executable(dpc_gemm_threaded main.cc)

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(dpc_gemm_threaded PUBLIC Pti::pti_view)

target_include_directories(dpc_gemm_threaded
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils" "${PROJECT_SOURCE_DIR}/../samples_utilities")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_gemm_threaded  PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
