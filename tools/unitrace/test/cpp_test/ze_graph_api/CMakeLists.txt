include("${PROJECT_SOURCE_DIR}/../../../../build_utils/CMakeLists.txt")

project(ze_graph_api CXX)

add_executable(ze_graph_api main.cc)

target_include_directories(ze_graph_api PRIVATE "${PROJECT_SOURCE_DIR}/../../../../../utils")

# Include Level Zero headers with Graph extension from unitrace build
# The headers are in two locations - check both
target_include_directories(ze_graph_api PRIVATE
    "${PROJECT_SOURCE_DIR}/../../../build/level-zero/include"
    "${PROJECT_SOURCE_DIR}/../../../build"
    "${PROJECT_SOURCE_DIR}/../../../build/compute-runtime/level_zero/include")

if(WIN32)
  FindL0Library(ze_graph_api)
  FindL0Headers(ze_graph_api)
else()
  target_link_libraries(ze_graph_api PRIVATE ze_loader)
endif()

# Copy the graph_kernels SPV (add_one, multiply_two)
add_custom_command(TARGET ze_graph_api POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/graph_kernels.spv ${CMAKE_CURRENT_BINARY_DIR}/graph_kernels.spv)

add_test(NAME ze_grph_api COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../run_test.py -c ${CMAKE_SOURCE_DIR} ze_graph_api)
