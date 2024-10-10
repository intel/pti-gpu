include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})


project(PTI_Samples_GPU_Instruction_Count CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-g")

add_library(memaccess_tool SHARED
  "${PROJECT_SOURCE_DIR}/tool.cc"
  "${PTI_CMAKE_MACRO_DIR}/../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/memaccess.cpp"
  )
target_include_directories(memaccess_tool
  PRIVATE "${PTI_CMAKE_MACRO_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(memaccess_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

add_subdirectory("${PTI_CMAKE_MACRO_DIR}/../utils/gtpin_utils" gtpin_tool_utils)
FindGTPinToolUtilsHeaders(memaccess_tool)
target_link_libraries(memaccess_tool gtpin_tool_utils)

FindGTPinLibrary(memaccess_tool)
FindGTPinHeaders(memaccess_tool)
FindGTPinUtils(memaccess_tool)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=memaccess_tool")
add_executable(memaccess "${PTI_CMAKE_MACRO_DIR}/../loader/loader.cc")
target_include_directories(memaccess
  PRIVATE "${PTI_CMAKE_MACRO_DIR}/../utils")
if(UNIX)
  target_link_libraries(memaccess
    dl)
endif()

# Installation

install(TARGETS memaccess memaccess_tool DESTINATION bin)
