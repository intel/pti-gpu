# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# Unitrace-owned header fetching.
#
# Unitrace builds against pinned upstream headers (Level Zero, compute-runtime,
# IGA) that it
# fetches into the build tree itself, instead of relying on whatever a host
# has installed. Each macro below creates the fetch target once and makes the
# given target compile against the fetched headers.

# Resolved at include time (CMAKE_CURRENT_LIST_DIR inside a macro would expand
# relative to the caller), so the macros work from any CMakeLists.txt.
get_filename_component(UNITRACE_FETCH_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../scripts" ABSOLUTE)

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
                      COMMAND "${Python_EXECUTABLE}" "${UNITRACE_FETCH_SCRIPTS_DIR}/get_iga_headers.py" ${IGA_INC_PATH} ${CMAKE_BINARY_DIR} ${IGA_COMMIT})
  endif()

  # BEFORE ensures the fetched headers win over any /usr/include/iga on the path.
  target_include_directories(${TARGET}
    BEFORE PUBLIC "${IGA_INC_PATH}")
  add_dependencies(${TARGET}
    unitrace_iga_headers)
endmacro()

# Fetch the pinned Level Zero API headers into the build tree and make TARGET
# compile against them.
#
#   UnitraceGetLevelZeroHeaders(<target> <level_zero_commit>)
#
# <level_zero_commit> is the oneapi-src/level-zero tag or revision to fetch
# (passed through to get_ze_headers.py). The headers are the input of the
# tracing-callback generator, so the pin is deliberately explicit.
macro(UnitraceGetLevelZeroHeaders TARGET LEVEL_ZERO_COMMIT)
  set(L0_INC_PATH "${CMAKE_BINARY_DIR}")

  # Create the fetch target once; the macro may be called for several targets.
  if (NOT TARGET unitrace_level_zero_headers)
    set(LEVEL_ZERO_HEADERS
      "${L0_INC_PATH}/level_zero/ze_api.h"
      "${L0_INC_PATH}/level_zero/zes_api.h"
      "${L0_INC_PATH}/level_zero/zet_api.h"
      "${L0_INC_PATH}/level_zero/zer_api.h"
      "${L0_INC_PATH}/level_zero/layers/zel_tracing_api.h"
      "${L0_INC_PATH}/level_zero/layers/zel_tracing_register_cb.h")
    add_custom_target(unitrace_level_zero_headers ALL DEPENDS ${LEVEL_ZERO_HEADERS})
    add_custom_command(OUTPUT ${LEVEL_ZERO_HEADERS}
      COMMAND "${Python_EXECUTABLE}" "${UNITRACE_FETCH_SCRIPTS_DIR}/get_ze_headers.py"
        "${L0_INC_PATH}" "${CMAKE_BINARY_DIR}" "${LEVEL_ZERO_COMMIT}"
      DEPENDS "${UNITRACE_FETCH_SCRIPTS_DIR}/get_ze_headers.py"
        "${UNITRACE_FETCH_SCRIPTS_DIR}/build_utils.py"
      VERBATIM)
  endif()

  # BEFORE ensures the fetched headers win over any /usr/include/level_zero.
  target_include_directories(${TARGET}
    BEFORE PUBLIC "${L0_INC_PATH}")
  add_dependencies(${TARGET}
    unitrace_level_zero_headers)
endmacro()

# Fetch the compute-runtime (NEO) headers that are not part of any public
# package into the build tree and make TARGET compile against them: the
# Level Zero driver extensions (driver_experimental/zex_*.h, ze_intel_gpu.h,
# ze_stypes.h) and the OpenCL tracing headers. The compute-runtime revision is
# pinned in get_compute_runtime_headers.py.
#
#   UnitraceGetComputeRuntimeHeaders(<target>)
macro(UnitraceGetComputeRuntimeHeaders TARGET)
  set(COMPUTE_RUNTIME_INC_PATH "${CMAKE_BINARY_DIR}")

  # Create the fetch target once; the macro may be called for several targets.
  if (NOT TARGET unitrace_compute_runtime_headers)
    file(GLOB COMPUTE_RUNTIME_ZEX_HEADERS
      "${COMPUTE_RUNTIME_INC_PATH}/level_zero/driver_experimental/zex_*.h")
    set(COMPUTE_RUNTIME_HEADERS
      "${COMPUTE_RUNTIME_INC_PATH}/CL/tracing_api.h"
      "${COMPUTE_RUNTIME_INC_PATH}/CL/tracing_types.h"
      "${COMPUTE_RUNTIME_INC_PATH}/CL/cl_ext_private.h"
      "${COMPUTE_RUNTIME_INC_PATH}/level_zero/ze_stypes.h"
      "${COMPUTE_RUNTIME_INC_PATH}/level_zero/ze_intel_gpu.h"
      "${COMPUTE_RUNTIME_INC_PATH}/level_zero/driver_experimental/zex_graph.h"
      ${COMPUTE_RUNTIME_ZEX_HEADERS})
    list(REMOVE_DUPLICATES COMPUTE_RUNTIME_HEADERS)
    add_custom_target(unitrace_compute_runtime_headers ALL DEPENDS ${COMPUTE_RUNTIME_HEADERS})
    add_custom_command(OUTPUT ${COMPUTE_RUNTIME_HEADERS}
      COMMAND "${Python_EXECUTABLE}" "${UNITRACE_FETCH_SCRIPTS_DIR}/get_compute_runtime_headers.py"
        "${COMPUTE_RUNTIME_INC_PATH}" "${CMAKE_BINARY_DIR}"
      DEPENDS "${UNITRACE_FETCH_SCRIPTS_DIR}/get_compute_runtime_headers.py"
        "${UNITRACE_FETCH_SCRIPTS_DIR}/build_utils.py"
      VERBATIM)
  endif()

  target_include_directories(${TARGET}
    PUBLIC "${COMPUTE_RUNTIME_INC_PATH}")
  add_dependencies(${TARGET}
    unitrace_compute_runtime_headers)
endmacro()
