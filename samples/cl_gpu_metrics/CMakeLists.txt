include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_GPU_Metrics CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_gpu_metrics SHARED
  "${PROJECT_SOURCE_DIR}/../loader/init.cc"
  tool.cc)
target_include_directories(clt_gpu_metrics
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../cl_hot_kernels")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_gpu_metrics
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
if(UNIX)
  target_link_libraries(clt_gpu_metrics
    pthread)
endif()

FindOpenCLLibrary(clt_gpu_metrics)
FindOpenCLHeaders(clt_gpu_metrics)

GetOpenCLTracingHeaders(clt_gpu_metrics)

GetMDHeaders(clt_gpu_metrics)
CheckForMDLibrary()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_gpu_metrics")
add_executable(cl_gpu_metrics "${PROJECT_SOURCE_DIR}/../loader/loader.cc")
target_include_directories(cl_gpu_metrics
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(UNIX)
  target_link_libraries(cl_gpu_metrics
    dl)
endif()