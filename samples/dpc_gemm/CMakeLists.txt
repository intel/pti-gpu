include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_DPC_GEMM CXX)
SetCompilerFlags()
SetBuildType()

CheckForDPCCompiler()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only")

add_executable(dpc_gemm main.cc)
target_include_directories(dpc_gemm
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_gemm
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()