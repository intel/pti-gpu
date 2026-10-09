project(grf CXX)

add_executable(grf grf.cpp)
add_sycl_to_target(grf)

install(TARGETS grf RUNTIME DESTINATION bin/grf COMPONENT grf)
