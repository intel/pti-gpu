include("../../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

macro(FindGTPinDevKitHeaders TARGET)
  target_include_directories(${TARGET}
    PUBLIC "${PROJECT_SOURCE_DIR}/include")
endmacro()

project(PTI_GTPin_tool_development_kit CXX)
SetCompilerFlags()
SetBuildType()

# GTPin tool development kit library
add_library(gtpin_dev_kit STATIC
  "${PROJECT_SOURCE_DIR}/src/filter.cc"
  "${PROJECT_SOURCE_DIR}/src/gtpin_capsules.cc"
  "${PROJECT_SOURCE_DIR}/src/gtpin_tool.cc"
  "${PROJECT_SOURCE_DIR}/src/prof_lib_gpu_gtpin.cc"
  "${PROJECT_SOURCE_DIR}/src/writer.cc"
  )
FindGTPinDevKitHeaders(gtpin_dev_kit)
set_property(TARGET gtpin_dev_kit PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(gtpin_dev_kit
  PRIVATE "${PROJECT_SOURCE_DIR}/../../../sdk/src/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(gtpin_dev_kit
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindGTPinLibrary(gtpin_dev_kit)
FindGTPinHeaders(gtpin_dev_kit)
FindGTPinUtils(gtpin_dev_kit)
