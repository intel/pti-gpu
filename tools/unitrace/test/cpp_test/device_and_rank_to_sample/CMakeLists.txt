include("${PROJECT_SOURCE_DIR}/../../../../build_utils/CMakeLists.txt")

project(device_and_rank_to_sample CXX)
add_executable(device_and_rank_to_sample empty.cpp empty_kernel.cpp)

if (WIN32)
    FindL0Library(device_and_rank_to_sample)
    FindL0Headers(device_and_rank_to_sample)
else()
    target_link_libraries(device_and_rank_to_sample PRIVATE -lze_loader)
endif()

