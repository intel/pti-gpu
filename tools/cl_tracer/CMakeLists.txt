include("../../samples/build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_CL_Tracer CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_tracer SHARED
  "${PROJECT_SOURCE_DIR}/../../samples/loader/init.cc"
  trace_guard.cc
  tool.cc)
target_include_directories(clt_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../samples/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_tracer
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(clt_tracer)
FindOpenCLHeaders(clt_tracer)

GetOpenCLTracingHeaders(clt_tracer)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_tracer")
add_executable(cl_tracer "${PROJECT_SOURCE_DIR}/../../samples/loader/loader.cc")
target_include_directories(cl_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../samples/utils")
if(UNIX)
  target_link_libraries(cl_tracer
    dl)
endif()