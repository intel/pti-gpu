project(grf CXX)

add_executable(grf grf.cpp)
add_sycl_to_target(grf)

add_test(NAME grf COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} grf)
