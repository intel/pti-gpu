include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Tools_OneTrace CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(onetrace_tool SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../cl_tracer/trace_guard.cc"
  "${PROJECT_SOURCE_DIR}/../utils/correlator.cc"
  "${PROJECT_SOURCE_DIR}/../utils/logger.cc"
  tool.cc)
target_include_directories(onetrace_tool
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../cl_tracer"
  PRIVATE "${PROJECT_SOURCE_DIR}/../ze_tracer")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(onetrace_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

if(DEFINED CLOCK_HIGH_RESOLUTION)
  message(STATUS "High resolution clock will be used")
  target_compile_definitions(onetrace_tool
    PUBLIC CLOCK_HIGH_RESOLUTION=${CLOCK_HIGH_RESOLUTION})
endif()

FindOpenCLLibrary(onetrace_tool)
FindOpenCLHeaders(onetrace_tool)

GetOpenCLTracingHeaders(onetrace_tool)

FindL0Library(onetrace_tool)
FindL0Headers(onetrace_tool)

FindL0HeadersPath(onetrace_tool "${PROJECT_SOURCE_DIR}/../ze_tracer/gen_tracing_callbacks.py")

if(UNIX)
  FindDRMLibrary(onetrace_tool)
  CheckDRMHeaders(onetrace_tool)
endif()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=onetrace_tool")
add_executable(onetrace "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(onetrace
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(onetrace
    dl)
endif()

# Installation

install(TARGETS onetrace onetrace_tool DESTINATION bin)