# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# Unitrace-owned Level Zero loader support for the test workloads on Windows.
# (unitrace itself loads ze_loader at runtime and never links it; the Level
# Zero headers come from HeaderFetch.cmake on every platform.)
#
#   UnitraceFindL0Library(<target>)  - link TARGET against ze_loader.lib; if
#                                      only ze_loader.dll is installed, generate
#                                      an import library from it.

# Resolved at include time so the macro works from any CMakeLists.txt.
get_filename_component(UNITRACE_LEVELZERO_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../scripts" ABSOLUTE)

macro(UnitraceFindL0Library TARGET)
  if(NOT WIN32)
    message(FATAL_ERROR "UnitraceFindL0Library is Windows-only; link ze_loader directly elsewhere")
  endif()
  find_library(L0_LIB_PATH NAMES ze_loader PATHS ${CMAKE_LIBRARY_PATH})
  if(NOT L0_LIB_PATH)
    set(PATH_LIST $ENV{PATH})
    foreach(VALUE ${PATH_LIST})
      if(VALUE MATCHES "[S|s]ystem32*$")
        set(WIN_SYS_PATH "${VALUE}")
        break()
      endif()
    endforeach()
    file(GLOB L0_DLL_PATH "${WIN_SYS_PATH}/ze_loader.dll")
    if(NOT L0_DLL_PATH)
      message(FATAL_ERROR "Level Zero loader is not found. You may need to install oneAPI Level Zero loader.")
    endif()
    message(STATUS "Level Zero Loader is found at ${L0_DLL_PATH}")
    find_library(L0_LIB_PATH NAMES ze_loader PATHS ${SEARCH_LIB_PATH})
    if(NOT L0_LIB_PATH)
      if(NOT TARGET unitrace_level_zero_lib)
        add_custom_target(unitrace_level_zero_lib ALL DEPENDS "${CMAKE_BINARY_DIR}/ze_loader.lib")
        add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/ze_loader.lib"
          COMMAND "${Python_EXECUTABLE}" "${UNITRACE_LEVELZERO_SCRIPTS_DIR}/convert_dll_to_lib.py"
            "${CMAKE_BINARY_DIR}" "${L0_DLL_PATH}"
          DEPENDS "${L0_DLL_PATH}" "${UNITRACE_LEVELZERO_SCRIPTS_DIR}/convert_dll_to_lib.py"
          VERBATIM)
      endif()
      add_dependencies(${TARGET} unitrace_level_zero_lib)
      target_link_libraries(${TARGET} "${CMAKE_BINARY_DIR}/ze_loader.lib")
    endif()
  endif()
  if(L0_LIB_PATH)
    message(STATUS "Level Zero loader is found at ${L0_LIB_PATH}")
    target_link_libraries(${TARGET} "${L0_LIB_PATH}")
  endif()
endmacro()
