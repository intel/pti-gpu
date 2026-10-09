project(cl_gemm CXX)

add_executable(cl_gemm main.cc)
add_sycl_to_target(cl_gemm)

target_include_directories(cl_gemm PRIVATE "${PROJECT_SOURCE_DIR}/../../../../../utils")
  
target_link_libraries(cl_gemm PRIVATE OpenCL)

install(TARGETS cl_gemm RUNTIME DESTINATION bin/cl_gemm COMPONENT cl_gemm)
