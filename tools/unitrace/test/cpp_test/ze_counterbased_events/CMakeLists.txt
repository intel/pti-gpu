project(ze_counterbased_events CXX)

add_executable(ze_counterbased_events main.cpp)

target_include_directories(ze_counterbased_events PRIVATE "${PROJECT_SOURCE_DIR}/../.." "${PROJECT_SOURCE_DIR}/../../..")

if(WIN32)
  UnitraceFindL0Library(ze_counterbased_events)
  UnitraceGetLevelZeroHeaders(ze_counterbased_events ${LEVEL_ZERO_COMMIT_ID})
else()
  target_link_libraries(ze_counterbased_events PRIVATE ze_loader)
endif()

install(TARGETS ze_counterbased_events RUNTIME DESTINATION bin/ze_counterbased_events COMPONENT ze_counterbased_events)
