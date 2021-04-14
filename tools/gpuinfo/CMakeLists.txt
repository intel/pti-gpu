include("../../samples/build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_GPU_Info CXX)
SetCompilerFlags()
SetBuildType()

add_executable(gpuinfo main.cc)
target_include_directories(gpuinfo
  PRIVATE "${PROJECT_SOURCE_DIR}/../../samples/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(gpuinfo
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

if(UNIX)
  target_link_libraries(gpuinfo
    dl)
endif()

GetMDHeaders(gpuinfo)
CheckForMDLibrary()

install(TARGETS gpuinfo DESTINATION bin)