project(sycl_graph_with_v2_adaptor CXX)

add_executable(sycl_graph_with_v2_adaptor main.cpp)
add_sycl_to_target(sycl_graph_with_v2_adaptor)

add_test(NAME sycl_graph_with_v2_adaptor COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} sycl_graph_with_v2_adaptor)
set_property(TEST sycl_graph_with_v2_adaptor PROPERTY ENVIRONMENT "SYCL_UR_USE_LEVEL_ZERO_V2=1")
