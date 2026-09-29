project(grf CXX)

add_executable(grf grf.cpp)
add_sycl_to_target(grf)

