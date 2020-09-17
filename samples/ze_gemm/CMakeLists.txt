include("../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_L0_GEMM CXX)
SetCompilerFlags()
SetBuildType()

add_executable(ze_gemm main.cc)
target_include_directories(ze_gemm
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(ze_gemm
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

add_custom_command(TARGET ze_gemm PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                      ${PROJECT_SOURCE_DIR}/gemm.spv
                      ${CMAKE_BINARY_DIR}/gemm.spv)
add_custom_command(TARGET ze_gemm PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                      ${PROJECT_SOURCE_DIR}/gemm.cl
                      ${CMAKE_BINARY_DIR}/gemm.cl)

FindL0Library(ze_gemm)
FindL0Headers(ze_gemm)

if(UNIX)
  target_link_libraries(ze_gemm
    dl)
endif()