#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
option(PTI_RECOMPILE_TEST_KERNELS "Recompile test SPIR-V kernels" OFF)

function(add_spirv TEST_TARGET KERNEL_FILE_BASENAME)
  find_program(SPV_COMPILER NAMES clang)

  if (NOT SPV_COMPILER)
    set(SPV_COMPILER ${CMAKE_C_COMPILER})
  endif()

  set(_kernel_src_filename "${KERNEL_FILE_BASENAME}.cl")
  set(_kernel_src_file "${CMAKE_CURRENT_SOURCE_DIR}/${_kernel_src_filename}")
  set(_spirv_filename "${KERNEL_FILE_BASENAME}.spv")
  set(_spirv_file "${CMAKE_CURRENT_SOURCE_DIR}/${_spirv_filename}")

  # This should probably output to build directory. However, for now, we will
  # keep it in the source directory.
  if (NOT TARGET compile_${KERNEL_FILE_BASENAME})
    add_custom_target(
      compile_${KERNEL_FILE_BASENAME}
      COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${_kernel_src_filename} to ${_spirv_filename}"
      COMMAND ${SPV_COMPILER} -target spirv64 -Xclang -finclude-default-header
              "${_kernel_src_file}" -o "${_spirv_file}"
      DEPENDS "${_kernel_src_file}"
      VERBATIM
    )
  endif()

  if (PTI_RECOMPILE_TEST_KERNELS)
    add_dependencies(${TEST_TARGET} compile_${KERNEL_FILE_BASENAME})
  endif()

  add_custom_command(
    TARGET ${TEST_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_spirv_file}"
            "$<TARGET_FILE_DIR:${TEST_TARGET}>/${_spirv_filename}"
    VERBATIM)
endfunction()
