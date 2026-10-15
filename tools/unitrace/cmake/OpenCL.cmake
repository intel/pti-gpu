# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# Unitrace-owned OpenCL build support, used only when BUILD_WITH_OPENCL is on.
#
#   UnitraceFindOpenCLLibrary(<target>)  - locate the OpenCL ICD loader and
#                                          link TARGET against it (fatal if absent)
#   UnitraceFindOpenCLHeaders(<target>)  - use the installed Khronos headers if
#                                          they are recent enough, otherwise
#                                          fetch the pinned ones into the build
#                                          tree; sets CL_TARGET_OPENCL_VERSION

include(CheckIncludeFileCXX)
include(CheckTypeSize)

# Resolved at include time so the macros work from any CMakeLists.txt.
get_filename_component(UNITRACE_OPENCL_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../scripts" ABSOLUTE)

macro(UnitraceFindOpenCLLibrary TARGET)
  if(WIN32)
    if(NOT OpenCL_FOUND)
      find_package(OpenCL 2.1)
    endif()
  else()
    set(SEARCH_LIB_PATH "")
    if(DEFINED ENV{LD_LIBRARY_PATH} AND NOT "$ENV{LD_LIBRARY_PATH}" STREQUAL "")
      string(REPLACE ":" ";" SEARCH_LIB_PATH "$ENV{LD_LIBRARY_PATH}")
    endif()
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .so.1)
    find_library(OpenCL_LIBRARY NAMES OpenCL PATHS ${SEARCH_LIB_PATH})
  endif()
  if(EXISTS "${OpenCL_LIBRARY}")
    message(STATUS "OpenCL library is found at ${OpenCL_LIBRARY}")
    target_link_libraries(${TARGET} ${OpenCL_LIBRARY})
  else()
    message(FATAL_ERROR
      "OpenCL library is not found. You may need to install OpenCL ICD Loader or specify "
      "the path to the library as -DCMAKE_LIBRARY_PATH=<OpenCL_Library_Path>")
  endif()
endmacro()

macro(UnitraceFindOpenCLHeaders TARGET)
  if(WIN32)
    if(NOT OpenCL_FOUND)
      find_package(OpenCL 2.1)
    endif()
  else()
    check_include_file_cxx(CL/cl.h OpenCL_INCLUDE_DIRS)
    if(OpenCL_INCLUDE_DIRS)
      set(CMAKE_EXTRA_INCLUDE_FILES "CL/cl.h")
      check_type_size(cl_kernel_sub_group_info CL_KERNEL_SUB_GROUP_INFO_SIZE LANGUAGE CXX)
      unset(CMAKE_EXTRA_INCLUDE_FILES)
      if(NOT CL_KERNEL_SUB_GROUP_INFO_SIZE)
        set(OpenCL_INCLUDE_DIRS 0)
      endif()
    endif()
  endif()

  if(NOT OpenCL_INCLUDE_DIRS)
    set(OPENCL_INC_PATH "${CMAKE_BINARY_DIR}")
    if(NOT TARGET unitrace_cl_headers)
      message(STATUS "OpenCL headers are not found, will be downloaded automatically")
      set(OPENCL_HEADERS
        "${OPENCL_INC_PATH}/CL/cl.h"
        "${OPENCL_INC_PATH}/CL/cl_gl.h"
        "${OPENCL_INC_PATH}/CL/cl_version.h"
        "${OPENCL_INC_PATH}/CL/cl_platform.h"
        "${OPENCL_INC_PATH}/CL/cl_ext.h")
      add_custom_target(unitrace_cl_headers ALL DEPENDS ${OPENCL_HEADERS})
      add_custom_command(OUTPUT ${OPENCL_HEADERS}
        COMMAND "${Python_EXECUTABLE}" "${UNITRACE_OPENCL_SCRIPTS_DIR}/get_cl_headers.py"
          "${OPENCL_INC_PATH}" "${CMAKE_BINARY_DIR}"
        DEPENDS "${UNITRACE_OPENCL_SCRIPTS_DIR}/get_cl_headers.py"
          "${UNITRACE_OPENCL_SCRIPTS_DIR}/build_utils.py"
        VERBATIM)
    endif()
    target_include_directories(${TARGET} PUBLIC "${OPENCL_INC_PATH}")
    add_dependencies(${TARGET} unitrace_cl_headers)
  else()
    target_include_directories(${TARGET} PUBLIC ${OpenCL_INCLUDE_DIRS})
  endif()
  target_compile_options(${TARGET} PRIVATE -DCL_TARGET_OPENCL_VERSION=300)
endmacro()
