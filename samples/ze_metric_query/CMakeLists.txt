include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Metric_Query CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_metric_query SHARED "${PROJECT_SOURCE_DIR}/../loader/init.cc" tool.cc)
target_include_directories(zet_metric_query
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_metric_query
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(zet_metric_query)
FindL0Headers(zet_metric_query)

CheckForMDLibrary()
CheckForMetricsLibrary()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_metric_query")
add_executable(ze_metric_query "${PROJECT_SOURCE_DIR}/../loader/loader.cc")
target_include_directories(ze_metric_query
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(UNIX)
  target_link_libraries(ze_metric_query
    dl)
endif()