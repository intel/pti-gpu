project(graph CXX)

add_executable(graph graph.cpp)
add_sycl_to_target(graph)

add_test(NAME graph COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} graph)
