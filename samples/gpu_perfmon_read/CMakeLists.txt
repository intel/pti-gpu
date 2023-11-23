include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_GPU_PerfMon_Read CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(gput_perfmon_read SHARED "${PROJECT_SOURCE_DIR}/../../loader/init.cc" tool.cc)
target_include_directories(gput_perfmon_read
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(gput_perfmon_read
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindIGALibrary(gput_perfmon_read)
GetIGAHeaders(gput_perfmon_read)

FindGTPinLibrary_legacy(gput_perfmon_read)
GetGTPinHeaders_legacy(gput_perfmon_read)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=gput_perfmon_read")
add_executable(gpu_perfmon_read "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(gpu_perfmon_read
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(gpu_perfmon_read
    dl)
endif()