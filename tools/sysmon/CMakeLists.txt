include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Tools_System_Monitor CXX)
SetCompilerFlags()
SetBuildType()

if(NOT UNIX)
  message(FATAL_ERROR "Linux only is supported")
endif()

add_executable(sysmon main.cc)
target_include_directories(sysmon
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(sysmon
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(sysmon)
FindL0Headers(sysmon)

# Installation

install(TARGETS sysmon DESTINATION bin)