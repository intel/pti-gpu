project(config_node_test CXX)

# Unit test of the --metric-config file parser; needs no GPU.
add_executable(config_node_test config_node_test.cpp)
target_compile_features(config_node_test PRIVATE cxx_std_17)
target_include_directories(config_node_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/utils")
# The shipped example must stay valid.
target_compile_definitions(config_node_test PRIVATE
  EXAMPLE_METRIC_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/metricconfig/metric_config.txt")

install(TARGETS config_node_test RUNTIME DESTINATION bin/metric_config COMPONENT config_node_test)
