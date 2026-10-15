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
set(PERFETTO_SRC_SHA256 "e7b94164193323695a81013ee82e1678bf434fc9fbb79f03b50fe1027bc93b5b"
    CACHE STRING "sha256 of the google/perfetto source tarball at PERFETTO_TAG")
set(PERFETTO_PROTO_SHA256 "69515f6fa6c0073de20f08c7ddee1b54befa788847af956ad5f74d113340f4c3"
    CACHE STRING "Expected sha256 of perfetto_trace.proto")
set(PROTOBUF_TAG "v3.21.12" CACHE STRING "protobuf tag to build from source")

function(SetupPerfetto)
  # 1. Fetch the trace proto, pinned by sha256 (cached; re-download skipped on match).
  set(proto "${PROJECT_BINARY_DIR}/perfetto/perfetto_trace.proto")
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
            ${PROJECT_BINARY_DIR} ${PROTOBUF_TAG} "${CMAKE_GENERATOR}" "${CMAKE_MAKE_PROGRAM}"
            "${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE build_result)
  if (NOT build_result EQUAL 0)
    message(WARNING "Failed to build protobuf from source; building JSON-only.")
    set(BUILD_WITH_PERFETTO OFF PARENT_SCOPE)
    return()
  endif()

  # 3. Locate the from-source install
  set(install "${PROJECT_BINARY_DIR}/protobuf-install")
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

  # 4. Generate C++ from the trace schema, into its own dir so it can be a
  #    SYSTEM include (silences protobuf's generated-code warnings) without
  #    affecting unitrace's other generated headers in the binary dir.
  #    Per source proto when the pinned source tarball is available, else from
  #    the single amalgamated file (why: third_party/perfetto/VERSION).
  set(gen_dir "${PROJECT_BINARY_DIR}/perfetto_gen")
  file(MAKE_DIRECTORY "${gen_dir}")

  set(srcs "")
  set(tarball "${PROJECT_BINARY_DIR}/perfetto/perfetto-${PERFETTO_TAG}.tar.gz")
  if (NOT EXISTS "${tarball}")
    # No EXPECTED_HASH: a mismatch there is a fatal error, and an unavailable
    # or changed tarball must fall back to the single-file flow instead.
    file(DOWNLOAD
      "https://github.com/google/perfetto/archive/refs/tags/${PERFETTO_TAG}.tar.gz"
      "${tarball}"
      STATUS src_dl_status TLS_VERIFY ON)
    list(GET src_dl_status 0 src_dl_code)
    if (NOT src_dl_code EQUAL 0)
      file(REMOVE "${tarball}")
    endif()
  endif()
  if (EXISTS "${tarball}")
    file(SHA256 "${tarball}" _tarball_hash)
    if (NOT _tarball_hash STREQUAL "${PERFETTO_SRC_SHA256}")
      message(WARNING "perfetto source tarball hash mismatch "
                      "(${_tarball_hash}); ignoring it.")
      file(REMOVE "${tarball}")
    endif()
  endif()

  if (EXISTS "${tarball}")
    # Keyed by the source hash, so a changed pin is extracted afresh rather
    # than reusing a previous release's protos.
    string(SUBSTRING "${PERFETTO_SRC_SHA256}" 0 16 _src_key)
    set(src_root "${PROJECT_BINARY_DIR}/perfetto/src-${_src_key}")
    if (NOT EXISTS "${src_root}/protos")
      set(_extract_dir "${PROJECT_BINARY_DIR}/perfetto/extract")
      file(REMOVE_RECURSE "${_extract_dir}" "${src_root}")
      file(ARCHIVE_EXTRACT INPUT "${tarball}"
           DESTINATION "${_extract_dir}"
           PATTERNS "*/protos/*")
      file(GLOB _extracted "${_extract_dir}/perfetto-*")
      list(GET _extracted 0 _extracted_root)
      file(RENAME "${_extracted_root}" "${src_root}")
    endif()

    # The amalgamated proto's source list, in its original order.
    file(STRINGS "${proto}" _markers REGEX "Begin of protos/")
    set(proto_rel "")
    set(gen_cc "")
    foreach(_line IN LISTS _markers)
      string(REGEX MATCH "protos/[^ ]+\.proto" _rel "${_line}")
      if (_rel AND EXISTS "${src_root}/${_rel}")
        list(APPEND proto_rel "${_rel}")
        string(REGEX REPLACE "\.proto$" ".pb.cc" _cc "${_rel}")
        list(APPEND gen_cc "${gen_dir}/${_cc}")
      else()
        set(proto_rel "")  # any miss: use the single-file fallback
        break()
      endif()
    endforeach()

    if (proto_rel)
      # Chunked invocations with paths relative to the source root: a single
      # command listing all 253 files exceeds the Windows command-line limit
      # (ninja then falls back to a batch file, which cmd.exe cannot run).
      list(LENGTH proto_rel _n)
      set(_chunk_size 40)
      set(_i 0)
      while(_i LESS _n)
        math(EXPR _rem "${_n} - ${_i}")
        if (_rem LESS _chunk_size)
          set(_len ${_rem})
        else()
          set(_len ${_chunk_size})
        endif()
        list(SUBLIST proto_rel ${_i} ${_len} _chunk_rel)
        set(_chunk_out "")
        set(_chunk_dep "")
        foreach(_rel IN LISTS _chunk_rel)
          string(REGEX REPLACE "\.proto$" ".pb.cc" _cc "${_rel}")
          string(REGEX REPLACE "\.proto$" ".pb.h" _h "${_rel}")
          list(APPEND _chunk_out "${gen_dir}/${_cc}" "${gen_dir}/${_h}")
          list(APPEND _chunk_dep "${src_root}/${_rel}")
        endforeach()
        add_custom_command(
          OUTPUT ${_chunk_out}
          COMMAND "${protoc}" --proto_path=. --cpp_out=${gen_dir} ${_chunk_rel}
          WORKING_DIRECTORY "${src_root}"
          DEPENDS ${_chunk_dep} "${proto}"
          COMMENT "Generating perfetto trace protobuf sources (${_i}/${_n})")
        math(EXPR _i "${_i} + ${_len}")
      endwhile()
      # Sources keep including "perfetto_trace.pb.h"; in the split layout the
      # messages they use live in these generated headers. Drop a monolithic
      # .pb.cc left by an earlier single-file configure: beside the shim it
      # would make a later fallback look up to date and skip regeneration.
      file(REMOVE "${gen_dir}/perfetto_trace.pb.cc")
      file(WRITE "${gen_dir}/perfetto_trace.pb.h"
        "#pragma once\n"
        "#include \"protos/perfetto/trace/trace.pb.h\"\n"
        "#include \"protos/perfetto/trace/trace_packet.pb.h\"\n"
        "#include \"protos/perfetto/trace/track_event/track_event.pb.h\"\n")
      set(srcs ${gen_cc})
    endif()
  endif()

  if (NOT srcs)
    message(WARNING "perfetto source tarball unavailable; generating the "
                    "single amalgamated .pb.cc (slower to compile).")
    set(srcs "${gen_dir}/perfetto_trace.pb.cc")
    add_custom_command(
      OUTPUT "${srcs}" "${gen_dir}/perfetto_trace.pb.h"
      COMMAND "${protoc}" --proto_path=${PROJECT_BINARY_DIR}/perfetto
              --cpp_out=${gen_dir} ${proto}
      DEPENDS "${proto}")
  endif()

  # Silence warnings, and hide visibility: the .pb.cc's protobuf template symbols
  # must not be exported from the LD_PRELOADed unitrace_tool.so (they'd interpose
  # on a target's own protobuf). --exclude-libs covers the archive, not these TUs.
  if (MSVC)
    set_source_files_properties(${srcs} PROPERTIES COMPILE_OPTIONS "/w")
  else()
    set_source_files_properties(${srcs} PROPERTIES COMPILE_OPTIONS
      "-w;-fvisibility=hidden;-fvisibility-inlines-hidden")
  endif()

  set(PERFETTO_PROTO_SRCS ${srcs} PARENT_SCOPE)
  set(PERFETTO_GEN_DIR "${gen_dir}" PARENT_SCOPE)
  set(PROTOBUF_INCLUDE "${install}/include" PARENT_SCOPE)
  set(PROTOBUF_LIB "${lib}" PARENT_SCOPE)
endfunction()
