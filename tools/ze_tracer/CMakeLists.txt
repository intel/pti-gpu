include("../../samples/build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Tracer CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_tracer SHARED
  "${PROJECT_SOURCE_DIR}/../../samples/loader/init.cc"
  ze_correlator.cc
  tool.cc)
target_include_directories(zet_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../samples/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_tracer
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(zet_tracer)
FindL0Headers(zet_tracer)

FindL0HeadersPath(zet_tracer "${PROJECT_SOURCE_DIR}/gen_tracing_callbacks.py")

if(UNIX)
  FindDRMLibrary(zet_tracer)
  CheckDRMHeaders(zet_tracer)
endif()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_tracer")
add_executable(ze_tracer "${PROJECT_SOURCE_DIR}/../../samples/loader/loader.cc")
target_include_directories(ze_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../samples/utils")
if(UNIX)
  target_link_libraries(ze_tracer
    dl)
endif()