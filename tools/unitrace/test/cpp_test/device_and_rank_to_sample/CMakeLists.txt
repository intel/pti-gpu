project(device_and_rank_to_sample CXX)
add_executable(device_and_rank_to_sample empty.cpp empty_kernel.cpp)

if (WIN32)
    UnitraceFindL0Library(device_and_rank_to_sample)
    UnitraceGetLevelZeroHeaders(device_and_rank_to_sample ${LEVEL_ZERO_COMMIT_ID})
else()
    target_link_libraries(device_and_rank_to_sample PRIVATE -lze_loader)
endif()

install(TARGETS device_and_rank_to_sample RUNTIME DESTINATION bin/device_and_rank_to_sample COMPONENT device_and_rank_to_sample)
