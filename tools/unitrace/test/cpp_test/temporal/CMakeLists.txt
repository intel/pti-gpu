project(temporal CXX)

add_executable(temporal temporal.cpp)
add_sycl_to_target(temporal)

