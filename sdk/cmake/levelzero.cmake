#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/dependency_pins.cmake)

macro(GetLevelZero)
  if (NOT TARGET LevelZero::level-zero)
    # Need zelEnableTracingLayer
    message("-- Fetching L0: ${PTI_LEVELZERO_VERSION}")
    string(REPLACE "." ";" LZ_TMP_LIST ${PTI_LEVELZERO_VERSION})
    list(GET LZ_TMP_LIST 2 LZ_VER_PATCH)
    set(LZ_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps)

    if(FETCHCONTENT_BASE_DIR)
      set(LZ_BASE_DIR ${FETCHCONTENT_BASE_DIR})
    endif()

    if (NOT Git_FOUND)
      find_package(Git REQUIRED)
    endif()

    include(FetchContent)
    FetchContent_Declare(
      LevelZero
      GIT_REPOSITORY ${PTI_LEVELZERO_REPOSITORY}
      GIT_TAG ${PTI_LEVELZERO_COMMIT}
    )
    # Prevent content from automatically being installed with PTI
    FetchContent_GetProperties(LevelZero)
    if(NOT levelzero_POPULATED)
        FetchContent_Populate(LevelZero)
        # Add patch to L0 build
        file(WRITE "${levelzero_SOURCE_DIR}/VERSION_PATCH" ${LZ_VER_PATCH})
        add_subdirectory(${levelzero_SOURCE_DIR} ${levelzero_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    get_target_property(PTI_ZE_LOADER_RUNTIME_DIR ze_loader RUNTIME_OUTPUT_DIRECTORY)

    # Create new target to treat level zero loader as an external dependency.
    # This prevents it from being added to the export set.
    # (Basically treat as if including via find_package)
    add_library(pti_ze_loader INTERFACE IMPORTED)
    add_dependencies(pti_ze_loader ze_tracing_layer ze_validation_layer)

    set(PTI_LZ_COMPILE_OPTIONS
        $<$<CXX_COMPILER_ID:IntelLLVM>:
            -Wno-error
            -Wno-unused-parameter
            -Wno-cast-function-type-mismatch
            -Wno-extra-semi
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,19.0.0>:-Wno-variadic-macro-arguments-omitted>
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /wd6285 /wd6246 /wd6031 /wd6386
            $<$<CONFIG:Release>:/wd4702 /wd6385 /wd6386>
        >
        $<$<CXX_COMPILER_ID:GNU>:
            -Wno-error
            -Wno-unused-parameter
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,8.0.0>:-Wno-extra-semi>
        >
        $<$<CXX_COMPILER_ID:Clang>:
            -Wno-error
            -Wno-unused-parameter
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,3.0.0>:-Wno-extra-semi>
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,19.0.0>:-Wno-variadic-macro-arguments-omitted>
        >
    )

    # Silence Warnings from Level Zero Loader. Allows us to better detect PTI
    # warnings and errors.
    set_target_properties(ze_loader ze_tracing_layer ze_null ze_validation_layer level_zero_utils
      PROPERTIES
        COMPILE_OPTIONS "${PTI_LZ_COMPILE_OPTIONS}"
        RUNTIME_OUTPUT_DIRECTORY "${PTI_ZE_LOADER_RUNTIME_DIR}/loader")

    # Pull Headers out of source tree and add them to level_zero/
    # This allows us to keep the normal way to include level zero
    file(GLOB_RECURSE L0_DL_HEADERS
        LIST_DIRECTORIES TRUE
        "${levelzero_SOURCE_DIR}/include/*")

    file(COPY ${L0_DL_HEADERS}
        DESTINATION
       ${LZ_BASE_DIR}/levelzero-headers/include/level_zero/)

    # Add new header path to our new target
    find_path(LZ_INCLUDE_DIR
      NAMES level_zero/ze_api.h
      HINTS ${LZ_BASE_DIR}/levelzero-headers
      PATH_SUFFIXES include
      NO_PACKAGE_ROOT_PATH
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )

    set_target_properties(pti_ze_loader PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES ${LZ_INCLUDE_DIR})

    target_link_libraries(pti_ze_loader INTERFACE
                            $<BUILD_INTERFACE:ze_loader>)
    add_library(LevelZero::level-zero ALIAS pti_ze_loader)
    add_library(LevelZero::headers INTERFACE IMPORTED)
    set_target_properties(
      LevelZero::headers
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LZ_INCLUDE_DIR}")
  endif()
endmacro()

macro(GetLevelZeroExtensions)
  if (NOT TARGET LevelZero::level-zero-ext)
    add_library(level-zero-ext INTERFACE)
    add_library(LevelZero::level-zero-ext ALIAS level-zero-ext)

    target_include_directories(level-zero-ext SYSTEM INTERFACE
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third-party/compute-runtime/level_zero/include>)

    target_link_libraries(level-zero-ext INTERFACE LevelZero::headers)
  endif()
endmacro()

if(NOT TARGET LevelZero::level-zero)
  find_package(LevelZero ${PTI_LEVELZERO_VERSION})
endif()

GetLevelZero()
GetLevelZeroExtensions()
