project(graph CXX)

add_executable(graph graph.cpp)
add_sycl_to_target(graph)

