include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_Debug_Info CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(zet_debug_info SHARED "${PROJECT_SOURCE_DIR}/../../loader/init.cc" tool.cc)
target_include_directories(zet_debug_info
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(zet_debug_info
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

add_subdirectory("${PTI_CMAKE_MACRO_DIR}/../utils/gpu_elf_parser" debug_info_parser)
FindPtiElfParserHeaders(zet_debug_info)
target_link_libraries(zet_debug_info debug_info_parser)

FindL0Library(zet_debug_info)
FindL0Headers(zet_debug_info)

FindIGALibrary(zet_debug_info)
GetIGAHeaders(zet_debug_info)

GetIGCHeaders(zet_debug_info)
GetGmmHeaders(zet_debug_info)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=zet_debug_info")
add_executable(ze_debug_info "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(ze_debug_info
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(ze_debug_info
    dl)
endif()
