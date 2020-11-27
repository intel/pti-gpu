include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_DPC_Info CXX)
SetCompilerFlags()
SetBuildType()

CheckForDPCCompiler()

add_executable(dpc_info main.cc)
if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_info
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()