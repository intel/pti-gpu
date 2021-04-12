include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_Debug_Info CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(clt_debug_info SHARED "${PROJECT_SOURCE_DIR}/../loader/init.cc" tool.cc)
target_include_directories(clt_debug_info
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(clt_debug_info
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(clt_debug_info)
FindOpenCLHeaders(clt_debug_info)

GetOpenCLTracingHeaders(clt_debug_info)

FindIGALibrary(clt_debug_info)
GetIGAHeaders(clt_debug_info)

GetIGCHeaders(clt_debug_info)
GetGmmHeaders(clt_debug_info)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=clt_debug_info")
add_executable(cl_debug_info "${PROJECT_SOURCE_DIR}/../loader/loader.cc")
target_include_directories(cl_debug_info
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(UNIX)
  target_link_libraries(cl_debug_info
    dl)
endif()