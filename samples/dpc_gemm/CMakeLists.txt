include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

if(WIN32)
  set(CMAKE_CXX_COMPILER "dpcpp-cl.exe")
  set(CMAKE_GENERATOR_TOOLSET "Intel(R) oneAPI DPC++ Compiler")
else()
  set(CMAKE_CXX_COMPILER "dpcpp")
endif()

project(PTI_Samples_DPC_GEMM CXX)
SetCompilerFlags()
SetBuildType()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only")

add_executable(dpc_gemm main.cc)
target_include_directories(dpc_gemm
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_gemm
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()