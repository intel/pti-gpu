# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
set(PTI_GPU_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

macro(SetPtiGpuVersion)
  file(STRINGS "${PTI_GPU_ROOT_DIR}/VERSION" PTI_GPU_VERSION LIMIT_COUNT 1)
  set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${PTI_GPU_ROOT_DIR}/VERSION")
endmacro()

function(GetCurrentGitRev COMMIT_OUTPUT_VAR)
  if(NOT DEFINED ${COMMIT_OUTPUT_VAR} OR "${${COMMIT_OUTPUT_VAR}}" STREQUAL "")
    set(_OUT_TEMP "")
    set(_GIT_RESULT_TEMP 1)
    if(NOT Git_FOUND)
      find_package(Git)
    endif()
    if(Git_FOUND)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${PTI_GPU_ROOT_DIR}"
        OUTPUT_VARIABLE _OUT_TEMP
        RESULT_VARIABLE _GIT_RESULT_TEMP
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_GIT_RESULT_TEMP EQUAL 0)
        set(${COMMIT_OUTPUT_VAR}
            "${_OUT_TEMP}"
            PARENT_SCOPE)
      endif()
    endif()
  endif()
endfunction()
