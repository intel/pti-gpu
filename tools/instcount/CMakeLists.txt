include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})


project(PTI_Samples_GPU_Instruction_Count CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-g")

add_library(instcount_tool SHARED
  "${PROJECT_SOURCE_DIR}/tool.cc"
  "${PTI_CMAKE_MACRO_DIR}/../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/instcount.cpp"
  )
target_include_directories(instcount_tool
  PRIVATE "${PTI_CMAKE_MACRO_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(instcount_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

add_subdirectory("${PTI_CMAKE_MACRO_DIR}/../utils/gtpin_utils" gtpin_tool_utils)
FindGTPinToolUtilsHeaders(instcount_tool)
target_link_libraries(instcount_tool gtpin_tool_utils)

FindGTPinLibrary(instcount_tool)
FindGTPinHeaders(instcount_tool)
FindGTPinUtils(instcount_tool)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=instcount_tool")
add_executable(instcount "${PTI_CMAKE_MACRO_DIR}/../loader/loader.cc")
target_include_directories(instcount
  PRIVATE "${PTI_CMAKE_MACRO_DIR}/../utils")
if(UNIX)
  target_link_libraries(instcount
    dl)
endif()

# Installation

install(TARGETS instcount instcount_tool DESTINATION bin)
