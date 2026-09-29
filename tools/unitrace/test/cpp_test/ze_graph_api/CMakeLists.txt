include("${PROJECT_SOURCE_DIR}/../../../../build_utils/CMakeLists.txt")

project(ze_graph_api CXX)

add_executable(ze_graph_api main.cc)

target_compile_features(ze_graph_api PRIVATE cxx_std_17)

target_include_directories(ze_graph_api PRIVATE "${PROJECT_SOURCE_DIR}/../../../../../utils")

# Include Level Zero headers with Graph extension from unitrace build
# The headers are in two locations - check both.
# UNITRACE_BUILD_ROOT (forwarded by test_unitrace.py --unitrace-build-dir) takes
# precedence over the standalone tools/unitrace build location.
if(DEFINED UNITRACE_BUILD_ROOT)
  set(_unitrace_build "${UNITRACE_BUILD_ROOT}")
else()
  set(_unitrace_build "${PROJECT_SOURCE_DIR}/../../../build")
endif()
target_include_directories(ze_graph_api PRIVATE
    "${_unitrace_build}/level-zero/include"
    "${_unitrace_build}"
    "${_unitrace_build}/compute-runtime/level_zero/include")

if(WIN32)
  FindL0Library(ze_graph_api)
  FindL0Headers(ze_graph_api)
else()
  target_link_libraries(ze_graph_api PRIVATE ze_loader ${CMAKE_DL_LIBS})
endif()

# Copy the graph_kernels SPV (add_one, multiply_two)
add_custom_command(TARGET ze_graph_api POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/graph_kernels.spv ${CMAKE_CURRENT_BINARY_DIR}/graph_kernels.spv)

