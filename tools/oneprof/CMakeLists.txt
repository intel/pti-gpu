include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Tools_OneProf CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPTI_KERNEL_INTERVALS=1")
add_library(oneprof_tool SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../utils/correlator.cc"
  "${PROJECT_SOURCE_DIR}/../../utils/trace_guard.cc"
  tool.cc)
target_include_directories(oneprof_tool
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../ze_tracer"
  PRIVATE "${PROJECT_SOURCE_DIR}/../cl_tracer"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(oneprof_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(oneprof_tool)
FindOpenCLHeaders(oneprof_tool)

GetOpenCLTracingHeaders(oneprof_tool)

FindL0Library(oneprof_tool)
FindL0Headers(oneprof_tool)

CheckForMDLibrary(oneprof_tool)
CheckForMetricsLibrary()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=oneprof_tool")
add_executable(oneprof "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(oneprof
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(oneprof
    dl)
endif()

# Installation

install(TARGETS oneprof oneprof_tool DESTINATION bin)