include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Intercept CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_intercept SHARED "${PROJECT_SOURCE_DIR}/../loader/init.cc" tool.cc)
target_include_directories(zet_intercept
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_intercept
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(zet_intercept)
FindL0Headers(zet_intercept)

FindL0HeadersPath(zet_intercept "${PROJECT_SOURCE_DIR}/gen_tracing_header.py")

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_intercept")
add_executable(ze_intercept "${PROJECT_SOURCE_DIR}/../loader/loader.cc")
target_include_directories(ze_intercept
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(UNIX)
  target_link_libraries(ze_intercept
    dl)
endif()