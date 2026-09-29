include("${PROJECT_SOURCE_DIR}/../../../../build_utils/CMakeLists.txt")

project(ze_counterbased_events CXX)

add_executable(ze_counterbased_events main.cpp)

target_include_directories(ze_counterbased_events PRIVATE "${PROJECT_SOURCE_DIR}/../../../../../utils")

if(WIN32)
  FindL0Library(ze_counterbased_events)
  FindL0Headers(ze_counterbased_events)  
else()
  target_link_libraries(ze_counterbased_events PRIVATE ze_loader)
endif()

