project(conditional_trace CXX)

add_executable(conditional_trace main.cc)
add_sycl_to_target(conditional_trace)

target_include_directories(conditional_trace PRIVATE "${PROJECT_SOURCE_DIR}/../../..")
  
target_link_libraries(conditional_trace PRIVATE OpenCL)

install(TARGETS conditional_trace RUNTIME DESTINATION bin/conditional_trace COMPONENT conditional_trace)
