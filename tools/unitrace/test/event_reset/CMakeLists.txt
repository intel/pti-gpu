project(event_reset CXX)

add_executable(event_reset event_reset.cpp)

if(WIN32)
  FindL0Library(event_reset)
  FindL0Headers(event_reset)  
else()
  target_link_libraries(event_reset PRIVATE ze_loader)
endif()

add_test(NAME event_reset_immediate_cl COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} event_reset -- -im)
add_test(NAME event_reset_cl_with_fence COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} event_reset -- -1nicl -f)
add_test(NAME event_reset_cl_with_barrier COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} event_reset -- -1nicl -b)
add_test(NAME event_reset_cl_with_queue_sync COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} event_reset -- -1nicl)
