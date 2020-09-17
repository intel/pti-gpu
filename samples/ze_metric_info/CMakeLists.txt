include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Metric_Info CXX)
SetCompilerFlags()
SetBuildType()

add_executable(ze_metric_info main.cc)
target_include_directories(ze_metric_info
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(ze_metric_info
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(ze_metric_info)
FindL0Headers(ze_metric_info)

CheckForMDLibrary()
CheckForMetricsLibrary()

if(UNIX)
  target_link_libraries(ze_metric_info
    dl)
endif()