project(event_reset CXX)

add_executable(event_reset event_reset.cpp)

if(WIN32)
  FindL0Library(event_reset)
  FindL0Headers(event_reset)  
else()
  target_link_libraries(event_reset PRIVATE ze_loader)
endif()

install(TARGETS event_reset RUNTIME DESTINATION bin/event_reset COMPONENT event_reset)
