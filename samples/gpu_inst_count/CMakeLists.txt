include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_GPU_Instruction_Count CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(gput_inst_count SHARED "${PROJECT_SOURCE_DIR}/../../loader/init.cc" tool.cc)
target_include_directories(gput_inst_count
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(gput_inst_count
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindIGALibrary(gput_inst_count)
GetIGAHeaders(gput_inst_count)

FindGTPinLibrary(gput_inst_count)
GetGTPinHeaders(gput_inst_count)

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=gput_inst_count")
add_executable(gpu_inst_count "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(gpu_inst_count
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(gpu_inst_count
    dl)
endif()