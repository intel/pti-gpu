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
``LevelZero::ze_loader``
  Level Zero loader

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
``LevelZero_Loader_LIBRARY``
  The path to the LevelZero loader library.

#]=======================================================================]

# Based on tutorial found in CMake manual:
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html

# Level Zero provides
#   * level-zero.pc
#   * libze_loader.pc
find_package(PkgConfig)
pkg_check_modules(PC_level_zero QUIET level-zero)
pkg_check_modules(PC_ze_loader QUIET libze_loader)

find_path(
  LevelZero_INCLUDE_DIR
  NAMES level_zero/ze_api.h
  PATHS ${PC_level_zero_INCLUDE_DIRS}
        ${PC_ze_loader_INCLUDE_DIRS}
        ${CMAKE_INCLUDE_PATH}
)

find_library(
  LevelZero_LIBRARY
  NAMES ze_loader
  PATHS ${PC_level_zero_LIBRARY_DIRS}
        ${PC_ze_loader_LIBRARY_DIRS}
        ENV LD_LIBRARY_PATH
        ${CMAKE_LIBRARY_PATH}
)

find_library(
  LevelZero_Loader_LIBRARY
  NAMES ze_loader
  PATHS ${PC_ze_loader_LIBRARY_DIRS}
        ENV LD_LIBRARY_PATH
        ${CMAKE_LIBRARY_PATH}
)

get_filename_component(LevelZero_LIBRARY_DIR ${LevelZero_LIBRARY} DIRECTORY)
# AFAIK, version can only be found via pc file. This is part of a regular level
# zero install.
find_path(
  LevelZero_PkgConfig_DIR
  NAMES level-zero.pc
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
  file(READ "${LevelZero_PkgConfig_DIR}/level-zero.pc" LevelZero_PkgConfig)
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

if(LevelZero_FOUND AND NOT TARGET LevelZero::ze_loader)
  add_library(LevelZero::ze_loader UNKNOWN IMPORTED)
  set_target_properties(
    LevelZero::ze_loader
    PROPERTIES IMPORTED_LOCATION "${LevelZero_Loader_LIBRARY}"
               INTERFACE_COMPILE_OPTIONS "${PC_ze_loader_CFLAGS_OTHER}"
               INTERFACE_INCLUDE_DIRECTORIES "${LevelZero_INCLUDE_DIR}")
endif()

mark_as_advanced(LevelZero_INCLUDE_DIR LevelZero_LIBRARY LevelZero_Loader_LIBRARY)

if(DEFINED LevelZero_VERSION)
  set(LevelZero_VERSION_STRING ${LevelZero_VERSION})
endif()
