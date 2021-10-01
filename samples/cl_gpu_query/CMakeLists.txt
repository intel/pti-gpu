include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_GPU_Query CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_gpu_query SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../../utils/trace_guard.cc"
  tool.cc)
target_include_directories(clt_gpu_query
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_gpu_query
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(clt_gpu_query)
FindOpenCLHeaders(clt_gpu_query)

GetOpenCLTracingHeaders(clt_gpu_query)

GetMDHeaders(clt_gpu_query)
CheckForMDLibrary(clt_gpu_query)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_gpu_query")
add_executable(cl_gpu_query "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(cl_gpu_query
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(cl_gpu_query
    dl)
endif()