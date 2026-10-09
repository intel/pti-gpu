project(temporal CXX)

add_executable(temporal temporal.cpp)
add_sycl_to_target(temporal)

install(TARGETS temporal RUNTIME DESTINATION bin/temporal COMPONENT temporal)
