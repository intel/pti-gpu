cmake_minimum_required(VERSION 3.12)

project(PTI_Tools_System_Monitor CXX)

if(NOT UNIX)
  message(FATAL_ERROR "Linux only is supported")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "32-bit mode is not supported")
endif()

# Version reported by --version: the repository VERSION file when sysmon is built
# inside the PTI repository, otherwise SYSMON_VERSION (e.g. -DSYSMON_VERSION=1.0.0).
if(NOT SYSMON_VERSION AND EXISTS "${PROJECT_SOURCE_DIR}/../../VERSION")
  file(STRINGS "${PROJECT_SOURCE_DIR}/../../VERSION" SYSMON_VERSION LIMIT_COUNT 1)
endif()

add_executable(sysmon main.cc)
set_target_properties(sysmon PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
if(SYSMON_VERSION)
  target_compile_definitions(sysmon PRIVATE PTI_VERSION=${SYSMON_VERSION})
endif()

# Level Zero headers (level_zero/ze_api.h, level_zero/zes_api.h) and loader must be
# installed, e.g. the libze-dev package. cmake/FindLevelZero.cmake (a copy of the
# PTI SDK module) also searches pkg-config, CPATH, LIBRARY_PATH, LD_LIBRARY_PATH,
# CMAKE_INCLUDE_PATH and CMAKE_LIBRARY_PATH.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(LevelZero REQUIRED)
# The module reports Level Zero as found with the headers only; sysmon also needs the loader.
if(NOT LevelZero_LIBRARY)
  message(FATAL_ERROR
    "Level Zero loader not found. Install the Level Zero development package "
    "(e.g. libze-dev) or pass -DCMAKE_LIBRARY_PATH=<loader dir>.")
endif()
target_link_libraries(sysmon PRIVATE LevelZero::level-zero)

# Installation

install(TARGETS sysmon DESTINATION bin)
