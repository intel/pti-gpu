# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
#
# CPack configuration: one package per tool (TGZ, DEB, RPM on Linux; ZIP on
# Windows, untested).
# Expects PTI_GPU_ENABLED_TOOLS from the top-level CMakeLists.txt; each tool
# is installed as a component of the same name.

if(NOT PTI_PROJECT_LICENSE_FILE)
  if(EXISTS "${PROJECT_SOURCE_DIR}/LICENSE")
    set(PTI_PROJECT_LICENSE_FILE "${PROJECT_SOURCE_DIR}/LICENSE")
  endif()
endif()

if(NOT PTI_PROJECT_LICENSE_SPDX)
  set(PTI_PROJECT_LICENSE_SPDX "MIT")
endif()

# ################### Generic CPack Configuration ####################
include(ProcessorCount)
ProcessorCount(NPROC)
set(CPACK_THREADS ${NPROC})

set(CPACK_STRIP_FILES OFF)

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ;DEB")
  find_program(PTI_GPU_RPMBUILD rpmbuild)
  if(PTI_GPU_RPMBUILD)
    list(APPEND CPACK_GENERATOR "RPM")
  endif()
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_VENDOR "PTI-GPU fork maintainers")
set(CPACK_PACKAGE_CONTACT "${PROJECT_HOMEPAGE_URL}/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}. Tracing and profiling tools for Intel GPUs."
)
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
if(PTI_PROJECT_LICENSE_FILE)
  set(CPACK_RESOURCE_FILE_LICENSE "${PTI_PROJECT_LICENSE_FILE}")
endif()
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

# One package per tool component.
set(CPACK_COMPONENTS_ALL ${PTI_GPU_ENABLED_TOOLS})
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

# ################### Tool Descriptions ####################
set(PTI_PKG_cl_tracer_DESCRIPTION "OpenCL(TM) API and device activity tracer")
set(PTI_PKG_ze_tracer_DESCRIPTION
    "oneAPI Level Zero API and device activity tracer")
set(PTI_PKG_onetrace_DESCRIPTION
    "Host and device tracing tool for OpenCL(TM) and oneAPI Level Zero")
set(PTI_PKG_oneprof_DESCRIPTION "GPU HW metrics collection tool")
set(PTI_PKG_gpuinfo_DESCRIPTION "GPU device and metrics information tool")
set(PTI_PKG_instcount_DESCRIPTION
    "GTPin-based GPU kernel instruction counting tool")
set(PTI_PKG_memaccess_DESCRIPTION "GTPin-based GPU memory access tracing tool")
set(PTI_PKG_sysmon_DESCRIPTION "GPU system monitoring tool")

# ################### DEB / RPM Configuration ####################
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "${PTI_PROJECT_LICENSE_SPDX}")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)

foreach(_tool IN LISTS PTI_GPU_ENABLED_TOOLS)
  string(TOUPPER "${_tool}" _TOOL)
  set(CPACK_DEBIAN_${_TOOL}_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-${_tool}")
  set(CPACK_DEBIAN_${_TOOL}_DESCRIPTION "${PTI_PKG_${_tool}_DESCRIPTION}")
  set(CPACK_RPM_${_TOOL}_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-${_tool}")
  set(CPACK_RPM_${_TOOL}_PACKAGE_SUMMARY "${PTI_PKG_${_tool}_DESCRIPTION}")
endforeach()

# Hidden directories (.git, .github, editor and cache dirs, local-only dirs).
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.[^/]+/"
    "/venv/"
    "/build/"
    "/build-[^/]*/"
    "/__pycache__/"
    "\\.swp$"
    "${PROJECT_BINARY_DIR}/")

include(CPack) # After CPACK_*

# ################### Components ####################
cpack_add_component_group(
  Tools
  DISPLAY_NAME "PTI-GPU Tools"
  DESCRIPTION "Tracing and profiling tools for Intel GPUs")

foreach(_tool IN LISTS PTI_GPU_ENABLED_TOOLS)
  cpack_add_component(
    ${_tool}
    DISPLAY_NAME "${_tool}"
    DESCRIPTION "${PTI_PKG_${_tool}_DESCRIPTION}"
    GROUP Tools)
endforeach()
