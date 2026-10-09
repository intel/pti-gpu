project(graph CXX)

add_executable(graph graph.cpp)
add_sycl_to_target(graph)

install(TARGETS graph RUNTIME DESTINATION bin/graph COMPONENT graph)
