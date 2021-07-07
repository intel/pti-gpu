include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_CL_Tracer CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_tracer SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../utils/correlator.cc"
  trace_guard.cc
  cl_ext_collector.cc
  tool.cc)
target_include_directories(clt_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_tracer
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

if(DEFINED CLOCK_HIGH_RESOLUTION)
  message(STATUS "High resolution clock will be used")
  target_compile_definitions(clt_tracer
    PUBLIC CLOCK_HIGH_RESOLUTION=${CLOCK_HIGH_RESOLUTION})
endif()

FindOpenCLLibrary(clt_tracer)
FindOpenCLHeaders(clt_tracer)

GetOpenCLTracingHeaders(clt_tracer)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_tracer")
add_executable(cl_tracer "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(cl_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(cl_tracer
    dl)
endif()

# Installation

install(TARGETS cl_tracer clt_tracer DESTINATION bin)