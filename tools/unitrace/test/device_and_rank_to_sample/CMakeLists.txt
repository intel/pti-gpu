project(device_and_rank_to_sample CXX)
add_executable(device_and_rank_to_sample empty.cpp empty_kernel.cpp)

if (WIN32)
    FindL0Library(device_and_rank_to_sample)
    FindL0Headers(device_and_rank_to_sample)
else()
    target_link_libraries(device_and_rank_to_sample PRIVATE -lze_loader)
endif()

add_test(NAME mpi_test_4_ranks_filter_0_2 COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py
    --mpiexec -n 4 -t mpi_test_4_ranks_filter_0_2
    ${CMAKE_SOURCE_DIR}
    device_and_rank_to_sample
)

add_test(NAME mpi_test_1_rank_sample_device_0 COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py
    --mpiexec -n 1 -t mpi_test_1_rank_sample_device_0
    ${CMAKE_SOURCE_DIR}
    device_and_rank_to_sample
)

add_test(NAME mpi_test_1_rank_sample_device_256 COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py
    --mpiexec -n 1 -t mpi_test_1_rank_sample_device_256
    ${CMAKE_SOURCE_DIR}
    device_and_rank_to_sample
)
