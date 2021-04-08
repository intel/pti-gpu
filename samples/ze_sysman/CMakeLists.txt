include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Sysmon CXX)
SetCompilerFlags()
SetBuildType()

add_executable(ze_sysman main.cc)
target_include_directories(ze_sysman
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(ze_sysman
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(ze_sysman)
FindL0Headers(ze_sysman)