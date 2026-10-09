# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# Unitrace-owned IGA CMake helpers.
#
# This module is the home for anything IGA-related that Unitrace needs. Keeping
# it under tools/unitrace/cmake gives Unitrace full control, independent of the
# shared build_utils macros (which other samples still rely on).

# Fetch the pinned IGA API headers into the build tree and make TARGET compile
# against them.
#
#   UnitraceGetIGAHeaders(<target> <iga_commit>)
#
# <iga_commit> is the intel-graphics-compiler revision to fetch (passed through
# to get_iga_headers.py) so the pinned commit lives in Unitrace's CMake rather
# than hardcoded in the Python script.
#
# Notes:
#   - We ALWAYS fetch and use our own headers. We deliberately do NOT probe the
#     system include paths and skip fetching when iga/*.h already exist there:
#     an installed IGA package may ship older headers under /usr/include/iga
#     that predate IGA_XE2 (XE_VER(2,0)), and building against those fails with
#     "IGA_XE2 not defined".
#   - The include path is added with BEFORE so the fetched headers win over any
#     stale system copy on the include search order.
#   - Only the C API headers are needed at build time; the iga64 shared library
#     is loaded at runtime via LibraryLoader (dlopen/LoadLibrary).
macro(UnitraceGetIGAHeaders TARGET IGA_COMMIT)
  set(IGA_INC_PATH "${CMAKE_BINARY_DIR}")
  RequirePythonInterp()

  # Create the fetch target once; the macro may be called for several targets.
  if (NOT TARGET unitrace_iga_headers)
    add_custom_target(unitrace_iga_headers ALL
                      DEPENDS ${IGA_INC_PATH}/iga/iga.h
                              ${IGA_INC_PATH}/iga/iga_types_ext.hpp
                              ${IGA_INC_PATH}/iga/iga_types_swsb.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_ops.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_enums.hpp)
    add_custom_command(OUTPUT ${IGA_INC_PATH}/iga/iga.h
                              ${IGA_INC_PATH}/iga/iga_types_ext.hpp
                              ${IGA_INC_PATH}/iga/iga_types_swsb.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_ops.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_enums.hpp
                      COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/get_iga_headers.py" ${IGA_INC_PATH} ${CMAKE_BINARY_DIR} ${IGA_COMMIT})
  endif()

  # BEFORE ensures the fetched headers win over any /usr/include/iga on the path.
  target_include_directories(${TARGET}
    BEFORE PUBLIC "${IGA_INC_PATH}")
  add_dependencies(${TARGET}
    unitrace_iga_headers)
endmacro()
