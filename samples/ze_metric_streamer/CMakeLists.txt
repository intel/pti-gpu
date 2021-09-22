include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Metric_Tracer CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_metric_streamer SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  tool.cc)
target_include_directories(zet_metric_streamer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../ze_hot_kernels")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_metric_streamer
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

if(UNIX)
  target_link_libraries(zet_metric_streamer
    pthread)
endif()

FindL0Library(zet_metric_streamer)
FindL0Headers(zet_metric_streamer)

CheckForMDLibrary(zet_metric_streamer)
CheckForMetricsLibrary()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_metric_streamer")
add_executable(ze_metric_streamer "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(ze_metric_streamer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(ze_metric_streamer
    dl)
endif()