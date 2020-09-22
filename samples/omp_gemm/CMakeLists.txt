include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenMP_GEMM CXX)
SetCompilerFlags()
SetBuildType()

CheckForIntelCompiler()

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Qnextgen /Qopenmp")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fiopenmp -fopenmp-targets=spir64")
endif()

add_executable(omp_gemm main.cc)
target_include_directories(omp_gemm
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(omp_gemm
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()