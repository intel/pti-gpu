# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLevelZero
-------

Finds the LevelZero library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``LevelZero::level-zero``
  Level Zero library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``LevelZero_FOUND``
  True if the system has the LevelZero library.
``LevelZero_INCLUDE_DIRS``
  Include directories needed to use LevelZero.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LevelZero_INCLUDE_DIR``
  The directory containing ``level_zero/ze_api.h``.
``LevelZero_LIBRARY``
  The path to the LevelZero library.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html

# Level Zero provides
#   * libze_loader.pc
find_package(PkgConfig)
pkg_check_modules(PC_ze_loader QUIET libze_loader)

find_path(
  LevelZero_INCLUDE_DIR
  NAMES level_zero/ze_api.h
  HINTS ENV CPATH
        ${PC_ze_loader_INCLUDE_DIRS}
  PATHS ${CMAKE_INCLUDE_PATH}
  PATH_SUFFIXES include
)

find_library(
  LevelZero_LIBRARY
  NAMES ze_loader
  HINTS ENV LIBRARY_PATH
        ${PC_ze_loader_LIBRARY_DIRS}
  PATHS ${CMAKE_LIBRARY_PATH}
        ENV LD_LIBRARY_PATH
  PATH_SUFFIXES lib
)

get_filename_component(LevelZero_LIBRARY_DIR ${LevelZero_LIBRARY} DIRECTORY)
# AFAIK, version can only be found via pc file. This is part of a regular level
# zero install.
find_path(
  LevelZero_PkgConfig_DIR
  NAMES libze_loader.pc
  HINTS ${LevelZero_LIBRARY_DIR}
  PATH_SUFFIXES pkgconfig
                lib/pkgconfig
                lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig
                ${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig
                lib64/pkgconfig
                lib64/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig
  NO_CMAKE_PATH
  NO_CMAKE_ENVIRONMENT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
  NO_CMAKE_SYSTEM_PATH
)

if(LevelZero_PkgConfig_DIR)
  file(READ "${LevelZero_PkgConfig_DIR}/libze_loader.pc" LevelZero_PkgConfig)
  string(REGEX MATCH "[0-9]+\.[0-9]+\.[0-9]+" LEVEL_ZERO_VER_FOUND "${LevelZero_PkgConfig}")
  set(LevelZero_VERSION ${LEVEL_ZERO_VER_FOUND})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LevelZero
  FOUND_VAR LevelZero_FOUND
  REQUIRED_VARS LevelZero_LIBRARY LevelZero_INCLUDE_DIR
  VERSION_VAR LevelZero_VERSION)

if(LevelZero_FOUND AND NOT TARGET LevelZero::level-zero)
  add_library(LevelZero::level-zero UNKNOWN IMPORTED)
  set_target_properties(
    LevelZero::level-zero
    PROPERTIES IMPORTED_LOCATION "${LevelZero_LIBRARY}"
               INTERFACE_COMPILE_OPTIONS "${PC_level_zero_CFLAGS_OTHER}"
               INTERFACE_INCLUDE_DIRECTORIES "${LevelZero_INCLUDE_DIR}")
endif()

mark_as_advanced(LevelZero_INCLUDE_DIR LevelZero_LIBRARY)

if(DEFINED LevelZero_VERSION)
  set(LevelZero_VERSION_STRING ${LevelZero_VERSION})
endif()
