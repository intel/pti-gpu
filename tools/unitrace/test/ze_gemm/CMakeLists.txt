include("${PROJECT_SOURCE_DIR}/../../../build_utils/CMakeLists.txt")

project(ze_gemm CXX)

add_executable(ze_gemm main.cc)

target_include_directories(ze_gemm PRIVATE "${PROJECT_SOURCE_DIR}/../../../../utils")

if(WIN32)
  FindL0Library(ze_gemm)
  FindL0Headers(ze_gemm)  
else()
  target_link_libraries(ze_gemm PRIVATE ze_loader)
endif()

add_custom_command(TARGET ze_gemm PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/gemm.spv ${CMAKE_BINARY_DIR}/ze_gemm/gemm.spv)

add_test(NAME ze_gemm COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} ze_gemm)
