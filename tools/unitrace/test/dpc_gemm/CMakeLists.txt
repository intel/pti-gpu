project(dpc_gemm CXX)

add_executable(dpc_gemm main.cc)
add_sycl_to_target(dpc_gemm)

target_include_directories(dpc_gemm PRIVATE "${PROJECT_SOURCE_DIR}/../../../../utils")

add_test(NAME dpc_gemm COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} dpc_gemm GPU)
