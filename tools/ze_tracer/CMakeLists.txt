include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Tracer CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_tracer SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../utils/correlator.cc"
  tool.cc)
target_include_directories(zet_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
target_compile_definitions(zet_tracer PUBLIC PTI_LEVEL_ZERO=1)
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_tracer
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindL0Library(zet_tracer)
FindL0Headers(zet_tracer)

FindL0HeadersPath(zet_tracer "${PROJECT_SOURCE_DIR}/gen_tracing_callbacks.py")

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_tracer")
add_executable(ze_tracer "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(ze_tracer
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(ze_tracer
    dl)
endif()

# Installation

install(TARGETS ze_tracer zet_tracer DESTINATION bin)