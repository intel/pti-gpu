# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
option(PTI_RECOMPILE_TEST_KERNELS "Recompile test SPIR-V kernels" OFF)

function(add_spirv TEST_TARGET KERNEL_FILE_BASENAME)
  find_program(SPV_COMPILER NAMES clang)

  if(NOT SPV_COMPILER)
    set(SPV_COMPILER ${CMAKE_C_COMPILER})
  endif()

  get_filename_component(_kernel_file_basename_ext "${KERNEL_FILE_BASENAME}"
                         LAST_EXT)

  if(_kernel_file_basename_ext STREQUAL ".spv")
    get_filename_component(_spirv_filename "${KERNEL_FILE_BASENAME}" NAME)
    if(IS_ABSOLUTE "${KERNEL_FILE_BASENAME}")
      set(_spirv_file "${KERNEL_FILE_BASENAME}")
    else()
      set(_spirv_file "${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL_FILE_BASENAME}")
    endif()
    set(_disable_compile ON)
  elseif(_kernel_file_basename_ext STREQUAL ".cl")
    message(
      FATAL_ERROR
        "Passing .cl files to add_spirv is not supported. It will not be"
        " compiled. Pass the .spv file instead or add a cl file without the"
        " extension and compile with PTI_RECOMPILE_TEST_KERNELS=ON.")
  else()
    set(_kernel_src_filename "${KERNEL_FILE_BASENAME}.cl")
    set(_kernel_src_file "${CMAKE_CURRENT_SOURCE_DIR}/${_kernel_src_filename}")
    set(_spirv_filename "${KERNEL_FILE_BASENAME}.spv")
    set(_spirv_file "${CMAKE_CURRENT_SOURCE_DIR}/${_spirv_filename}")
  endif()

  # This should probably output to build directory. However, for now, we will
  # keep it in the source directory.
  if(NOT TARGET compile_${KERNEL_FILE_BASENAME} AND NOT _disable_compile)
    add_custom_target(
      compile_${KERNEL_FILE_BASENAME}
      COMMAND ${CMAKE_COMMAND} -E echo
              "Compiling ${_kernel_src_filename} to ${_spirv_filename}"
      COMMAND ${SPV_COMPILER} -target spirv64 -Xclang -finclude-default-header
              "${_kernel_src_file}" -o "${_spirv_file}"
      DEPENDS "${_kernel_src_file}"
      VERBATIM)
  endif()

  if(PTI_RECOMPILE_TEST_KERNELS
     AND NOT _disable_compile
     AND TARGET compile_${KERNEL_FILE_BASENAME})
    add_dependencies(${TEST_TARGET} compile_${KERNEL_FILE_BASENAME})
  endif()

  add_custom_command(
    TARGET ${TEST_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_spirv_file}"
            "$<TARGET_FILE_DIR:${TEST_TARGET}>/${_spirv_filename}"
    VERBATIM)
endfunction()
