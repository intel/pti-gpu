# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzz targets for unitrace (SDL505 — Perform Fuzz Testing)
#
# The targets include the unitrace sources the parsers live in (ze_collector.h,
# ze_metrics.h, utils_ze.h, utils_host.h, chromelogger.h, unitrace.cc), so they build as part of
# unitrace, with its include paths and generated headers. No GPU is needed:
#   cmake -S tools/unitrace -B build-fuzz -DUNITRACE_BUILD_FUZZ=ON \
#         -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#         -DBUILD_WITH_OMP=0 -DBUILD_WITH_PERFETTO=OFF
#   cmake --build build-fuzz --target unitrace_fuzz
#
# Each target is a standalone libFuzzer binary.

# Clang, or oneAPI's clang-based icpx (IntelLLVM).
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|IntelLLVM")
  message(FATAL_ERROR "The fuzz targets need clang (libFuzzer); set CMAKE_CXX_COMPILER=clang++")
endif()

set(UNITRACE_SRC "${PROJECT_SOURCE_DIR}/src")

# Common compiler flags for all fuzz targets. coverage_report.sh builds with
# UNITRACE_FUZZ_SANITIZERS=fuzzer and coverage flags in UNITRACE_FUZZ_EXTRA_FLAGS.
set(UNITRACE_FUZZ_SANITIZERS "fuzzer,address,undefined" CACHE STRING "-fsanitize= for the fuzz targets")
set(UNITRACE_FUZZ_EXTRA_FLAGS "" CACHE STRING "Extra compile and link flags for the fuzz targets")
set(FUZZ_COMPILE_FLAGS "-fsanitize=${UNITRACE_FUZZ_SANITIZERS} -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1 ${UNITRACE_FUZZ_EXTRA_FLAGS}")
set(FUZZ_LINK_FLAGS    "-fsanitize=${UNITRACE_FUZZ_SANITIZERS} ${UNITRACE_FUZZ_EXTRA_FLAGS}")

# The include paths and definitions unitrace and the collector library build with,
# plus the generated headers in the build directory.
set(FUZZ_INCLUDE_DIRS
  "${CMAKE_BINARY_DIR}"
  "${UNITRACE_SRC}"
  "${UNITRACE_SRC}/levelzero"
  "${UNITRACE_SRC}/opencl"
  "${UNITRACE_SRC}/itt"
  "$<TARGET_PROPERTY:unitrace,INCLUDE_DIRECTORIES>"
  "$<TARGET_PROPERTY:unitrace_tool,INCLUDE_DIRECTORIES>"
)
set(FUZZ_DEFINITIONS
  UNITRACE_FUZZ
  "$<TARGET_PROPERTY:unitrace,COMPILE_DEFINITIONS>"
)

add_custom_target(unitrace_fuzz)

# ---------------------------------------------------------------------------
# Helper: declare a fuzz target
# ---------------------------------------------------------------------------
function(add_fuzz_target name)
  cmake_parse_arguments(FT "" "" "SOURCES;INCLUDES;LIBS" ${ARGN})
  add_executable(${name} ${FT_SOURCES} "${UNITRACE_SRC}/utils/logger_factory.cc")
  target_include_directories(${name} PRIVATE ${FUZZ_INCLUDE_DIRS} ${FT_INCLUDES})
  target_compile_definitions(${name} PRIVATE ${FUZZ_DEFINITIONS})
  set_target_properties(${name} PROPERTIES
    COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
    LINK_FLAGS    "${FUZZ_LINK_FLAGS}"
  )
  target_compile_features(${name} PRIVATE cxx_std_17)
  target_link_libraries(${name} ${CMAKE_DL_LIBS} pthread ${FT_LIBS})
  # logger_factory.cc uses <filesystem>: link the library unitrace links for it, if
  # the toolchain needs one (the parent CMakeLists.txt checks).
  if (FILESYSTEM_NEEDS_LIBSTDCXXFS)
    target_link_libraries(${name} stdc++fs)
  elseif (FILESYSTEM_NEEDS_LIBCXXFS)
    target_link_libraries(${name} c++fs)
  endif()
  # Where the collector's libraries are (e.g. Xptifw_LIBRARY_DIR for -lxptifw), for
  # the targets that link them.
  target_link_directories(${name} PRIVATE "$<TARGET_PROPERTY:unitrace_tool,LINK_DIRECTORIES>")
  # The generated headers (.gen, commit hash, IGA) come from the unitrace build.
  add_dependencies(${name} unitrace unitrace_tool)
  add_dependencies(unitrace_fuzz ${name})
endfunction()

# ---------------------------------------------------------------------------
# 1. ELF64 / ZeBin parser chain (elf64_reader.h, kernel_extractor.h,
#    source_resolver.h) — highest-value binary format target
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_elf64
  SOURCES fuzz_elf64.cc
)

# ---------------------------------------------------------------------------
# 2. Binary metric query file parser (.q files) — ze_metrics.h
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_metric_query
  SOURCES fuzz_metric_query.cc
)

# ---------------------------------------------------------------------------
# 3. Binary stall sampling file parser (.t files) — ze_metrics.h
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_stall_sampling
  SOURCES fuzz_stall_sampling.cc
)

# ---------------------------------------------------------------------------
# 4. CLI argument parser — cli_options.h
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_cli_args
  SOURCES fuzz_cli_args.cc
)

# ---------------------------------------------------------------------------
# 5. Kernel filter file parser — ze_collector.h ParseFilterList()
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_kernel_filter
  SOURCES fuzz_kernel_filter.cc
)

# ---------------------------------------------------------------------------
# 6. KMD trace data parser — unitrace.cc ReadKmdEvent(), ParseKmdLine(), KmdEventToJson()
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_kmd_trace
  SOURCES fuzz_kmd_trace.cc
)

# ---------------------------------------------------------------------------
# 7. Kernel properties (.kprops) and kernel times (.ktime) readers
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_kernel_props
  SOURCES fuzz_kernel_props.cc
)

# ---------------------------------------------------------------------------
# 8. --devices-to-sample / --ranks-to-sample and the MPI launcher environment
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_sample_lists
  SOURCES fuzz_sample_lists.cc
)

# ---------------------------------------------------------------------------
# 9. Names and arguments written into the Chrome trace and the config log
# ---------------------------------------------------------------------------
# tracer.h brings the collectors chromelogger.h relies on, so link what the
# collector library links (OpenCL, ...).
add_fuzz_target(fuzz_trace_json
  SOURCES fuzz_trace_json.cc
  LIBS "$<TARGET_PROPERTY:unitrace_tool,LINK_LIBRARIES>"
)

# ---------------------------------------------------------------------------
# 10. Device indexes and the Chrome event buffer size from the environment
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_env_numbers
  SOURCES fuzz_env_numbers.cc
)

# ---------------------------------------------------------------------------
# 11. Metric value counts returned by the driver and the sampling timestamps
# ---------------------------------------------------------------------------
add_fuzz_target(fuzz_metric_samples
  SOURCES fuzz_metric_samples.cc
)
