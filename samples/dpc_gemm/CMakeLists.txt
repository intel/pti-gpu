include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

if(WIN32)
  set(CMAKE_CXX_COMPILER "dpcpp-cl.exe")
else()
  set(CMAKE_CXX_COMPILER "icpx")
endif()

project(PTI_Samples_DPC_GEMM CXX)
SetCompilerFlags()
SetBuildType()

add_executable(dpc_gemm main.cc)

# -g rather than -gline-tables-only: icpx ignores the latter for the SPIR-V
# device target, and ze_debug_info needs device line info for source mapping.
target_compile_options(dpc_gemm PUBLIC -fsycl -g)

# target_link_options CMake >= 3.13
set_target_properties(dpc_gemm PROPERTIES LINK_FLAGS "-fsycl -g")

target_include_directories(dpc_gemm
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_gemm
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
