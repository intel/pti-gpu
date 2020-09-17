include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_GPU_PerfMon_Set CXX)
SetCompilerFlags()
SetBuildType()

if(NOT UNIX)
  message(FATAL_ERROR "Linux only is supported")
endif()

add_executable(gpu_perfmon_set main.cc)
target_include_directories(gpu_perfmon_set
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(gpu_perfmon_set
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindDRMLibrary(gpu_perfmon_set)
CheckDRMHeaders(gpu_perfmon_set)