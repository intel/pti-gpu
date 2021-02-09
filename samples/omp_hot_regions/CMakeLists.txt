include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

if(WIN32)
  set(CMAKE_CXX_COMPILER "icl.exe")
else()
  set(CMAKE_CXX_COMPILER "icpx")
endif()

project(PTI_Samples_OpenMP_Hot_Regions CXX)
SetCompilerFlags()
SetBuildType()

CheckForOMPTHeaders()

add_library(omp_hot_regions SHARED tool.cc)
target_include_directories(omp_hot_regions
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(omp_hot_regions
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()