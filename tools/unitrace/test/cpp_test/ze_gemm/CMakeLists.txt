project(ze_gemm CXX)

add_executable(ze_gemm main.cc)

target_include_directories(ze_gemm PRIVATE "${PROJECT_SOURCE_DIR}/../.." "${PROJECT_SOURCE_DIR}/../../..")

if(WIN32)
  UnitraceFindL0Library(ze_gemm)
  UnitraceGetLevelZeroHeaders(ze_gemm ${LEVEL_ZERO_COMMIT_ID})
else()
  target_link_libraries(ze_gemm PRIVATE ze_loader)
endif()

add_custom_command(TARGET ze_gemm PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/gemm.spv ${CMAKE_BINARY_DIR}/ze_gemm/gemm.spv)

install(TARGETS ze_gemm RUNTIME DESTINATION bin/ze_gemm COMPONENT ze_gemm)
install(FILES ${PROJECT_SOURCE_DIR}/gemm.spv
        DESTINATION bin/ze_gemm COMPONENT ze_gemm)
