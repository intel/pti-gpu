project(temporal CXX)

add_executable(temporal temporal.cpp)
add_sycl_to_target(temporal)

add_test(NAME temporal COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} temporal)
