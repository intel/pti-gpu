include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Hot_Functions CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_hot_functions SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  tool.cc)
target_include_directories(zet_hot_functions
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_hot_functions
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(zet_hot_functions)
FindL0Headers(zet_hot_functions)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_hot_functions")
add_executable(ze_hot_functions "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(ze_hot_functions
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(ze_hot_functions
    dl)
endif()