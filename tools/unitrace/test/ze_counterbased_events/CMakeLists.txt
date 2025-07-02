include("${PROJECT_SOURCE_DIR}/../../../build_utils/CMakeLists.txt")

project(ze_counterbased_events CXX)

add_executable(ze_counterbased_events main.cpp)

target_include_directories(ze_counterbased_events PRIVATE "${PROJECT_SOURCE_DIR}/../../../../utils")

if(WIN32)
  FindL0Library(ze_counterbased_events)
  FindL0Headers(ze_counterbased_events)  
else()
  target_link_libraries(ze_counterbased_events PRIVATE ze_loader)
endif()

add_test(NAME ze_counterbased_events
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py -s "chrome-device-logging" -t ${CMAKE_CURRENT_SOURCE_DIR}/check_outfiles.py ${CMAKE_SOURCE_DIR} ze_counterbased_events)
