# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
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

# TODO: Figure out whether this is desired or not. We have not stripped symbols
# in the past, so for now, this will be kept OFF. Note, leaving this ON is
# incompatible with CPACK_DEBIAN_<GROUP>_DEBUGINFO_PACKAGE and
# CPACK_RPM_<GROUP>_DEBUGINFO_PACKAGE.
set(CPACK_STRIP_FILES OFF)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_VENDOR "Intel Corporation")
set(CPACK_PACKAGE_CONTACT "Intel Corporation <secure@intel.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}. Tools and libraries for Intel trace and profiling."
)
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
if(PTI_PROJECT_LICENSE_FILE)
  set(CPACK_RESOURCE_FILE_LICENSE "${PTI_PROJECT_LICENSE_FILE}")
endif()
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_WELCOME "${CPACK_RESOURCE_FILE_README}")

set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)

# ################### Package Descriptions ####################
set(PTI_PKG_RUNTIME_DESCRIPTION
    "PTI runtime libraries for collecting Intel GPU data")
set(PTI_PKG_DEVELOPMENT_DESCRIPTION
    "PTI development files (headers, etc...) for building with PTI")
set(PTI_PKG_ONEAPI_DESCRIPTION
    "PTI Intel(R) oneAPI integration files for oneAPI installation")
set(PTI_PKG_UNITRACE_GROUP_DESCRIPTION
    "Intel(R) GPU Unitrace profiler tool for collecting Intel GPU data")
set(PTI_PKG_UNITRACE_SCRIPTS_GROUP_DESCRIPTION
    "Intel(R) GPU Unitrace analysis scripts for analyzing data collected by Unitrace"
)

# ################### DEBIAN CPack Configuration ####################
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# pti-gpu-sdk
set(CPACK_DEBIAN_PTI_RUNTIME_GROUP_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-sdk")
set(CPACK_DEBIAN_PTI_RUNTIME_GROUP_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_PTI_RUNTIME_GROUP_DESCRIPTION "${PTI_PKG_RUNTIME_DESCRIPTION}")

# pti-gpu-sdk-dev
set(CPACK_DEBIAN_PTI_DEVELOPMENT_GROUP_PACKAGE_NAME
    "${CPACK_PACKAGE_NAME}-sdk-dev")
set(CPACK_DEBIAN_PTI_DEVELOPMENT_GROUP_PACKAGE_DEPENDS
    "${CPACK_PACKAGE_NAME}-sdk (= ${CPACK_PACKAGE_VERSION})")
set(CPACK_DEBIAN_PTI_DEVELOPMENT_GROUP_DESCRIPTION
    "${PTI_PKG_DEVELOPMENT_DESCRIPTION}")

# pti-gpu-sdk-oneapi
set(CPACK_DEBIAN_PTI_ONEAPI_GROUP_PACKAGE_NAME
    "${CPACK_PACKAGE_NAME}-sdk-oneapi")
set(CPACK_DEBIAN_PTI_ONEAPI_GROUP_PACKAGE_ARCHITECTURE "all")
set(CPACK_DEBIAN_PTI_ONEAPI_GROUP_PACKAGE_DEPENDS
    "${CPACK_PACKAGE_NAME}-sdk (= ${CPACK_PACKAGE_VERSION})")
set(CPACK_DEBIAN_PTI_ONEAPI_GROUP_DESCRIPTION "${PTI_PKG_ONEAPI_DESCRIPTION}")

# pti-gpu-unitrace
set(CPACK_DEBIAN_UNITRACE_GROUP_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-unitrace")
set(CPACK_DEBIAN_UNITRACE_GROUP_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_UNITRACE_GROUP_DESCRIPTION
    "${PTI_PKG_UNITRACE_GROUP_DESCRIPTION}")

# pti-gpu-unitrace-scripts
set(CPACK_DEBIAN_UNITRACE_SCRIPTS_GROUP_PACKAGE_NAME
    "${CPACK_PACKAGE_NAME}-unitrace-scripts")
set(CPACK_DEBIAN_UNITRACE_SCRIPTS_GROUP_PACKAGE_ARCHITECTURE "all")
set(CPACK_DEBIAN_UNITRACE_SCRIPTS_GROUP_PACKAGE_DEPENDS
    "${CPACK_PACKAGE_NAME}-unitrace (= ${CPACK_PACKAGE_VERSION}), python3 (>= 3.9)"
)
set(CPACK_DEBIAN_UNITRACE_SCRIPTS_GROUP_PACKAGE_RECOMMENDS
    "python3-pandas, python3-matplotlib")
set(CPACK_DEBIAN_UNITRACE_SCRIPTS_GROUP_DESCRIPTION
    "${PTI_PKG_UNITRACE_SCRIPTS_GROUP_DESCRIPTION}")

# ################### RPM CPack Configuration ####################
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-sdk")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_MAIN_COMPONENT Pti_Runtime_Group)
set(CPACK_RPM_PACKAGE_LICENSE "${PTI_PROJECT_LICENSE_SPDX}")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)

set(CPACK_RPM_PTI_RUNTIME_GROUP_PACKAGE_SUMMARY
    "Intel(R) Profiling Tools Interfaces libraries")
set(CPACK_RPM_PTI_RUNTIME_GROUP_PACKAGE_DESCRIPTION
    "${PTI_PKG_RUNTIME_DESCRIPTION}")
set(CPACK_RPM_PTI_RUNTIME_GROUP_DEBUGINFO_PACKAGE ON)

# pti-gpu-sdk-devel
set(CPACK_RPM_PTI_DEVELOPMENT_GROUP_PACKAGE_NAME
    "${CPACK_PACKAGE_NAME}-sdk-devel")
set(CPACK_RPM_PTI_DEVELOPMENT_GROUP_PACKAGE_SUMMARY
    "Intel(R) Profiling Tools Interfaces development files")
set(CPACK_RPM_PTI_DEVELOPMENT_GROUP_PACKAGE_DESCRIPTION
    "${PTI_PKG_DEVELOPMENT_DESCRIPTION}")
set(CPACK_RPM_PTI_DEVELOPMENT_GROUP_PACKAGE_REQUIRES
    "${CPACK_PACKAGE_NAME}-sdk = ${CPACK_PACKAGE_VERSION}")

# pti-gpu-sdk-oneapi
set(CPACK_RPM_PTI_ONEAPI_GROUP_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-sdk-oneapi")
set(CPACK_RPM_PTI_ONEAPI_GROUP_PACKAGE_ARCHITECTURE "noarch")
set(CPACK_RPM_PTI_ONEAPI_GROUP_PACKAGE_SUMMARY
    "Intel(R) oneAPI files for Intel(R) Profiling Tools Interfaces")
set(CPACK_RPM_PTI_ONEAPI_GROUP_PACKAGE_DESCRIPTION
    "${PTI_PKG_ONEAPI_DESCRIPTION}")
set(CPACK_RPM_PTI_ONEAPI_GROUP_PACKAGE_REQUIRES
    "${CPACK_PACKAGE_NAME}-sdk = ${CPACK_PACKAGE_VERSION}")

# pti-gpu-unitrace
set(CPACK_RPM_UNITRACE_GROUP_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-unitrace")
set(CPACK_RPM_UNITRACE_GROUP_PACKAGE_SUMMARY
    "Unitrace profiler for Intel(R) GPUs")
set(CPACK_RPM_UNITRACE_GROUP_PACKAGE_DESCRIPTION
    "${PTI_PKG_UNITRACE_GROUP_DESCRIPTION}")
set(CPACK_RPM_UNITRACE_GROUP_DEBUGINFO_PACKAGE ON)

# pti-gpu-unitrace-scripts
set(CPACK_RPM_UNITRACE_SCRIPTS_GROUP_PACKAGE_NAME
    "${CPACK_PACKAGE_NAME}-unitrace-scripts")
set(CPACK_RPM_UNITRACE_SCRIPTS_GROUP_PACKAGE_ARCHITECTURE "noarch")
set(CPACK_RPM_UNITRACE_SCRIPTS_GROUP_PACKAGE_SUMMARY
    "Analysis scripts for data collected by unitrace")
set(CPACK_RPM_UNITRACE_SCRIPTS_GROUP_PACKAGE_DESCRIPTION
    "${PTI_PKG_UNITRACE_SCRIPTS_GROUP_DESCRIPTION}")
set(CPACK_RPM_UNITRACE_SCRIPTS_GROUP_PACKAGE_REQUIRES
    "${CPACK_PACKAGE_NAME}-unitrace = ${CPACK_PACKAGE_VERSION}, python3 >= 3.9")

set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/"
    "/\\.github/"
    "/\\.vscode/"
    "/\\.vs/"
    "/\\.vim/"
    "/\\.cache/"
    "/\\.venv/"
    "/venv/"
    "/build/"
    "/build-[^/]*/"
    "/wbr/"
    "/out/"
    "/drop_raw/"
    "/dist/"
    "/__pycache__/"
    "\\.swp$"
    "${PROJECT_BINARY_DIR}/")

include(CPack) # After CPACK_*

# ################### Component Groups ####################
cpack_add_component_group(
  Pti_Runtime_Group
  DISPLAY_NAME "PTI Runtime"
  DESCRIPTION "PTI Runtime Libraries for collecting Level Zero and SYCL data.")

cpack_add_component_group(
  Pti_Development_Group
  DISPLAY_NAME "PTI Development"
  DESCRIPTION "Files (headers, etc...) needed to build against PTI")

cpack_add_component_group(
  Pti_oneAPI_Group
  DISPLAY_NAME "PTI oneAPI-specifics"
  DESCRIPTION "oneAPI installation files")

cpack_add_component_group(
  Unitrace_Group
  DISPLAY_NAME "Unitrace"
  DESCRIPTION "Unitrace Tool Binaries")

cpack_add_component_group(
  Unitrace_Scripts_Group
  DISPLAY_NAME "Unitrace Analysis Scripts"
  DESCRIPTION "Scripts that analyze the data collected by unitrace")

# ################### Components ####################
cpack_add_component(
  Pti_Runtime
  DISPLAY_NAME "Libraries"
  DESCRIPTION "Libraries needed at runtime"
  GROUP Pti_Runtime_Group)

cpack_add_component(
  Pti_Doc
  DISPLAY_NAME "Documentation"
  DESCRIPTION "Documentation related files"
  GROUP Pti_Runtime_Group
  DEPENDS Pti_Runtime)

cpack_add_component(
  Pti_Development
  DISPLAY_NAME "Development"
  DESCRIPTION "Header files + configuration files"
  GROUP Pti_Development_Group
  DEPENDS Pti_Runtime)

cpack_add_component(
  Pti_oneAPI
  DISPLAY_NAME "oneAPI-specifics"
  DESCRIPTION "files related to oneAPI installation"
  GROUP Pti_oneAPI_Group
  DEPENDS Pti_Runtime)

cpack_add_component(
  Pti_Bom
  DISPLAY_NAME "oneAPI Bill of Materials"
  DESCRIPTION "Bill of materials generated for oneAPI drops"
  GROUP Pti_oneAPI_Group
  DEPENDS Pti_oneAPI
  HIDDEN)

cpack_add_component(
  Unspecified
  DISPLAY_NAME "Unspecified"
  DESCRIPTION "Files installed without an explicit component"
  GROUP Pti_oneAPI_Group
  HIDDEN)

cpack_add_component(
  Unitrace_Runtime
  DISPLAY_NAME "Unitrace Binaries"
  DESCRIPTION "Unitrace tool and runtime"
  GROUP Unitrace_Group)

cpack_add_component(
  Unitrace_Config
  DISPLAY_NAME "Unitrace Configuration Files"
  DESCRIPTION "Unitrace configuration files"
  GROUP Unitrace_Group
  DEPENDS Unitrace_Runtime)

cpack_add_component(
  Unitrace_Scripts
  DISPLAY_NAME "Unitrace Analysis Scripts"
  DESCRIPTION "Unitrace scripts for analysis of collected data"
  GROUP Unitrace_Scripts_Group
  DEPENDS Unitrace_Runtime)
