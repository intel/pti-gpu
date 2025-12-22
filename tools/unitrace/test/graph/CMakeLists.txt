project(graph CXX)

add_executable(graph graph.cpp)
add_sycl_to_target(graph)

add_test(NAME graph COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} graph)
add_test(NAME selective_kernels COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py -e ${CMAKE_CURRENT_SOURCE_DIR}/check_outfiles.py ${CMAKE_SOURCE_DIR} graph)
