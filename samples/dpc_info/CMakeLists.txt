include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

if(WIN32)
  set(CMAKE_CXX_COMPILER "dpcpp-cl.exe")
  set(CMAKE_GENERATOR_TOOLSET "Intel(R) oneAPI DPC++ Compiler")
else()
  set(CMAKE_CXX_COMPILER "icpx")
endif()

project(PTI_Samples_DPC_Info CXX)
SetCompilerFlags()
SetBuildType()

add_executable(dpc_info main.cc)

if(NOT WIN32)
  target_compile_options(dpc_info PUBLIC -fsycl)
  set_target_properties(dpc_info PROPERTIES LINK_FLAGS "-fsycl")
endif()

if(CMAKE_INCLUDE_PATH)
  target_include_directories(dpc_info
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
