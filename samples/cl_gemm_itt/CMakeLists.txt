include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_OpenCL_GEMM_ITT CXX)
SetCompilerFlags()
SetBuildType()

add_executable(cl_gemm_itt main.cc ittnotify.cc)
target_include_directories(cl_gemm_itt
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(cl_gemm_itt
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(cl_gemm_itt)
FindOpenCLHeaders(cl_gemm_itt)

GetITT(cl_gemm_itt)

if(UNIX)
  target_link_libraries(cl_gemm_itt
    dl)
endif()