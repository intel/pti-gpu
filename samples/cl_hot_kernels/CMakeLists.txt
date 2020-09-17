include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_Hot_Kernels CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_hot_kernels SHARED "${PROJECT_SOURCE_DIR}/../loader/init.cc" tool.cc)
target_include_directories(clt_hot_kernels
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_hot_kernels
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(clt_hot_kernels)
FindOpenCLHeaders(clt_hot_kernels)

GetOpenCLTracingHeaders(clt_hot_kernels)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_hot_kernels")
add_executable(cl_hot_kernels "${PROJECT_SOURCE_DIR}/../loader/loader.cc")
target_include_directories(cl_hot_kernels
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(UNIX)
  target_link_libraries(cl_hot_kernels
    dl)
endif()