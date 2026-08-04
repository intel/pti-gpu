#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Perfetto protobuf trace output support for unitrace.
#
# Call SetupPerfetto() only when BUILD_WITH_PERFETTO is ON. It fetches the
# Perfetto trace proto (pinned by sha256), builds protoc + libprotobuf from source
# into the build tree and generates the C++ bindings. On any failure it degrades to
# JSON-only by setting BUILD_WITH_PERFETTO OFF
#
# On success it sets the following, which the caller wires
# into its target:
#   PERFETTO_PROTO_SRCS   - generated .pb.cc to add to the target's sources
#   PERFETTO_GEN_DIR      - dir of the generated .pb.{h,cc}
#   PROTOBUF_INCLUDE      - libprotobuf headers
#   PROTOBUF_LIB          - the static libprotobuf to link

# Tags / pinned hash live next to this file in third_party/perfetto/VERSION.
set(PERFETTO_TAG "v56.1" CACHE STRING "google/perfetto tag for perfetto_trace.proto")
set(PERFETTO_PROTO_SHA256 "69515f6fa6c0073de20f08c7ddee1b54befa788847af956ad5f74d113340f4c3"
    CACHE STRING "Expected sha256 of perfetto_trace.proto")
set(PROTOBUF_TAG "v3.21.12" CACHE STRING "protobuf tag to build from source")

function(SetupPerfetto)
  # 1. Fetch the trace proto, pinned by sha256 (cached; re-download skipped on match).
  set(proto "${CMAKE_BINARY_DIR}/perfetto/perfetto_trace.proto")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/google/perfetto/${PERFETTO_TAG}/protos/perfetto/trace/perfetto_trace.proto"
    "${proto}"
    EXPECTED_HASH SHA256=${PERFETTO_PROTO_SHA256}
    STATUS dl_status TLS_VERIFY ON)
  list(GET dl_status 0 dl_code)
  if (NOT dl_code EQUAL 0)
    list(GET dl_status 1 dl_msg)
    message(WARNING "Perfetto proto fetch failed (${dl_msg}); building JSON-only.")
    set(BUILD_WITH_PERFETTO OFF PARENT_SCOPE)
    return()
  endif()

  # 2. Build protoc + libprotobuf from source (skips if already built). Forward
  #    our generator and build type so the sub-build matches the parent (keeps
  #    Ninja and the same MSVC runtime on Windows).
  execute_process(
    COMMAND "${Python_EXECUTABLE}" ${PROJECT_SOURCE_DIR}/scripts/get_protobuf.py
            ${CMAKE_BINARY_DIR} ${PROTOBUF_TAG} "${CMAKE_GENERATOR}" "${CMAKE_MAKE_PROGRAM}"
            "${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE build_result)
  if (NOT build_result EQUAL 0)
    message(WARNING "Failed to build protobuf from source; building JSON-only.")
    set(BUILD_WITH_PERFETTO OFF PARENT_SCOPE)
    return()
  endif()

  # 3. Locate the from-source install
  set(install "${CMAKE_BINARY_DIR}/protobuf-install")
  set(protoc "${install}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")
  set(lib "${install}/lib/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if (NOT EXISTS "${lib}")
    set(lib "${install}/lib64/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}")  # some distros install to lib64
  endif()
  if (NOT EXISTS "${protoc}" OR NOT EXISTS "${lib}")
    message(WARNING "protobuf install incomplete (protoc or libprotobuf missing); building JSON-only.")
    set(BUILD_WITH_PERFETTO OFF PARENT_SCOPE)
    return()
  endif()

  # 4. Generate C++ from the proto, into its own dir so it can be a SYSTEM
  #    include (silences protobuf's generated-code warnings) without affecting
  #    unitrace's other generated headers in the binary dir.
  set(gen_dir "${CMAKE_BINARY_DIR}/perfetto_gen")
  set(srcs "${gen_dir}/perfetto_trace.pb.cc")
  file(MAKE_DIRECTORY "${gen_dir}")
  add_custom_command(
    OUTPUT "${srcs}" "${gen_dir}/perfetto_trace.pb.h"
    COMMAND "${protoc}" --proto_path=${CMAKE_BINARY_DIR}/perfetto
            --cpp_out=${gen_dir} ${proto}
    DEPENDS "${proto}")

  # Silence warnings, and hide visibility: the .pb.cc's protobuf template symbols
  # must not be exported from the LD_PRELOADed unitrace_tool.so (they'd interpose
  # on a target's own protobuf). --exclude-libs covers the archive, not this TU.
  if (MSVC)
    set_source_files_properties("${srcs}" PROPERTIES COMPILE_OPTIONS "/w")
  else()
    set_source_files_properties("${srcs}" PROPERTIES COMPILE_OPTIONS
      "-w;-fvisibility=hidden;-fvisibility-inlines-hidden")
  endif()

  set(PERFETTO_PROTO_SRCS "${srcs}" PARENT_SCOPE)
  set(PERFETTO_GEN_DIR "${gen_dir}" PARENT_SCOPE)
  set(PROTOBUF_INCLUDE "${install}/include" PARENT_SCOPE)
  set(PROTOBUF_LIB "${lib}" PARENT_SCOPE)
endfunction()
