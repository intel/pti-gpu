include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_GEMM_Instrumented CXX)
SetCompilerFlags()
SetBuildType()

add_executable(cl_gemm_inst main.cc)
target_include_directories(cl_gemm_inst
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(cl_gemm_inst
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(cl_gemm_inst)
FindOpenCLHeaders(cl_gemm_inst)