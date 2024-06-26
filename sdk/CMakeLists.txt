set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(macros)
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

SetCompilerFlags()
project(
  pti
  VERSION ${PTI_VERSION}
  LANGUAGES C CXX)

ProjectIsTopLevel()
RemoveNDebugFlag()
SetBuildType()
CheckIfSyclIsAvailable()
GetSpdlog()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

if(PROJECT_IS_TOP_LEVEL)
  AddFormatTarget()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_VISIBILITY_PRESET "hidden")

set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

option(PTI_BUILD_TESTING "Build PTI tests" ${PROJECT_IS_TOP_LEVEL})
option(PTI_BUILD_SAMPLES "Build samples with library" ${PROJECT_IS_TOP_LEVEL})
option(PTI_INSTALL "Install library when invoking --install" ON)
option(PTI_ENABLE_LOGGING "Enable logging for Pti" OFF)
option(PTI_DEBUG "Enable code helping to debug Pti" OFF)
option(PTI_SANITIZE "Enable Sanitizer Pti" OFF)
option(PTI_FUZZ "Enable Fuzz Pti" OFF)

include(GNUInstallDirs)
set(PTI_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pti")
set(PTI_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(PTI_INSTALL_LIB_DIR "${CMAKE_INSTALL_LIBDIR}")
set(PTI_INSTALL_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
set(PTI_INSTALL_LIC_DIR "${CMAKE_INSTALL_DOCDIR}")
set(PTI_API_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

CheckSOVersion(${PTI_API_SOVERSION})

set(PTI_COMPILE_FLAGS_EXPR
    $<$<CXX_COMPILER_ID:IntelLLVM>:-Wall
    -Wextra
    -Wformat
    -Wformat-security
    $<$<CONFIG:Debug>:-fstack-protector>
    $<$<CONFIG:Release>:
    $<$<BOOL:${PROJECT_IS_TOP_LEVEL}>:-Werror
    -Werror=format-security>
    -fstack-protector-strong>>
    $<$<CXX_COMPILER_ID:Clang>:-Wall
    -Wextra
    -Wformat
    -Wformat-security
    $<$<CONFIG:Debug>:-fstack-protector>
    $<$<CONFIG:Release>:
    $<$<BOOL:${PROJECT_IS_TOP_LEVEL}>:-Werror
    -Werror=format-security>
    -fstack-protector-strong>>
    $<$<CXX_COMPILER_ID:GNU>:-Wall
    -Wextra
    -Wformat
    -Wformat-security
    $<$<CONFIG:Debug>:-fstack-protector>
    $<$<CONFIG:Release>:
    $<$<BOOL:${PROJECT_IS_TOP_LEVEL}>:-Werror
    -Werror=format-security>
    -fstack-protector-strong>>)

set(PTI_LINK_FLAGS_EXPR
    $<$<CXX_COMPILER_ID:IntelLLVM>:
    $<$<CONFIG:Debug>:>
    $<$<CONFIG:Release>:-Wl,-z,relro,-z,now,-z,noexecstack>>
    $<$<CXX_COMPILER_ID:Clang>:
    $<$<CONFIG:Debug>:>
    $<$<CONFIG:Release>:-Wl,-z,relro,-z,now,-z,noexecstack>>
    $<$<CXX_COMPILER_ID:GNU>:
    $<$<CONFIG:Release>:-Wl,-z,relro,-z,now,-z,noexecstack>>)

set(PTI_DEFINE_FLAGS_EXPR
    $<$<CXX_COMPILER_ID:IntelLLVM>:$<$<NOT:$<BOOL:${PTI_SANITIZE}>>:_FORTIFY_SOURCE=2>>
    $<$<CXX_COMPILER_ID:Clang>:$<$<NOT:$<BOOL:${PTI_SANITIZE}>>:_FORTIFY_SOURCE=2>>
    $<$<CXX_COMPILER_ID:GNU>:$<$<NOT:$<BOOL:${PTI_SANITIZE}>>:_FORTIFY_SOURCE=2>>
)

configure_file(${PROJECT_SOURCE_DIR}/include/pti/pti_version.h.in
               ${PROJECT_BINARY_DIR}/include/pti/pti_version.h)

find_package(Xpti)

# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
add_library(pti SHARED "${PROJECT_SOURCE_DIR}/src/pti.cc")
add_library(Pti::pti ALIAS pti)

# We might want to make our own, but for now use CMake's
# https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html
include(GenerateExportHeader)
generate_export_header(pti EXPORT_FILE_NAME
                       ${PROJECT_BINARY_DIR}/include/pti/pti_export.h)

target_include_directories(
  pti
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
         "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
         "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
         "$<INSTALL_INTERFACE:${PTI_INSTALL_INCLUDE_DIR}>"
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
          "${PROJECT_SOURCE_DIR}/src/syclpi" "${PROJECT_SOURCE_DIR}/src/utils"
          "${PROJECT_SOURCE_DIR}/src/loader")

set_target_properties(
  pti
  PROPERTIES VERSION "${PROJECT_VERSION}"
             SOVERSION "${PTI_API_SOVERSION}"
             POSITION_INDEPENDENT_CODE ON)

target_compile_options(pti PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(pti PRIVATE ${PTI_LINK_FLAGS_EXPR})
target_compile_definitions(
  pti PRIVATE ${PTI_DEFINE_FLAGS_EXPR}
              $<$<TARGET_EXISTS:Xpti::xpti>:PTI_TRACE_SYCL>)

# Why version 1.14.0?
# It is an IPEX requirement for PTI to link against the LTS version of the
# Level Zero Loader.
# However, for best results, we recommend installing, Level Zero Loader >=
# 1.16.15.
# Notice about loader versions:
#  * 1.15.8 supports dynamic tracing i.e. ze[Enable/Disable]TracingLayer.
#  * 1.16.1 supports Level-Zero core Introspection API introduced in Spec 1.9.
#   *  Notice: this supports the new API, but not the new feature (local
#      collection). One needs to install a compatible Level Zero driver to
#      use the feature. Currently supported in the Intel GPU rolling release
#      drivers.
#  * 1.16.15 fixes a critical bug in ze[Enable/Disable]TracingLayer.
if(NOT PROJECT_IS_TOP_LEVEL)
  find_package(LevelZero 1.14.0 REQUIRED)
else()
  find_package(LevelZero 1.14.0)
endif()

if(NOT TARGET LevelZero::level-zero)
  GetLevelZero()
endif()

find_package(Threads REQUIRED)

add_library(pti_view SHARED "${PROJECT_SOURCE_DIR}/src/pti_view.cc"
                            "${PROJECT_SOURCE_DIR}/src/pti_version.cc")
add_library(Pti::pti_view ALIAS pti_view)
# add_library(pti_callback SHARED "..") add_library(pti_event SHARED "..")
# add_library(pti_metric SHARED "..") add_library(pti_profiling SHARED "..") ..
target_include_directories(
  pti_view
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
         "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
         "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
         "$<INSTALL_INTERFACE:${PTI_INSTALL_INCLUDE_DIR}>"
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/syclpi"
          "${PROJECT_SOURCE_DIR}/src/levelzero"
          "${PROJECT_SOURCE_DIR}/src/utils" "${PROJECT_SOURCE_DIR}/src/loader")

set_target_properties(
  pti_view
  PROPERTIES VERSION "${PROJECT_VERSION}"
             SOVERSION "${PTI_API_SOVERSION}"
             POSITION_INDEPENDENT_CODE ON)

target_compile_options(pti_view PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(pti_view PRIVATE ${PTI_LINK_FLAGS_EXPR})

target_compile_definitions(
  pti_view
  PRIVATE $<$<TARGET_EXISTS:Xpti::xpti>:PTI_TRACE_SYCL>
          $<$<TARGET_EXISTS:LevelZero::level-zero>:PTI_LEVEL_ZERO=1>
          ${PTI_DEFINE_FLAGS_EXPR}
          $<IF:$<BOOL:${PTI_ENABLE_LOGGING}>,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF>)

if(PTI_DEBUG)
  add_compile_definitions(PTI_DEBUG)
endif()

target_link_libraries(
  pti_view
  PUBLIC Threads::Threads
  PRIVATE $<TARGET_NAME_IF_EXISTS:Xpti::xpti> LevelZero::level-zero
          spdlog::spdlog_header_only)

FindHeadersPath(
  pti_view "${PROJECT_SOURCE_DIR}/src/levelzero/gen_tracing_callbacks.py"
  "tracing.gen" gen_tracing_header LevelZero::level-zero)

target_link_libraries(
  pti
  PUBLIC pti_view
  PRIVATE spdlog::spdlog_header_only $<TARGET_NAME_IF_EXISTS:Xpti::xpti>)

include(CTest)
if(BUILD_TESTING AND PTI_BUILD_TESTING)
  add_subdirectory(test)
endif()

export(
  TARGETS pti pti_view
  FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/PtiTargets.cmake"
  NAMESPACE Pti::)

if(PTI_INSTALL)
  include(CMakePackageConfigHelpers)

  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/PtiConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion)

  install(
    FILES "${PROJECT_SOURCE_DIR}/PtiConfig.cmake"
          "${PROJECT_BINARY_DIR}/PtiConfigVersion.cmake"
    COMPONENT Pti_Development
    DESTINATION "${PTI_INSTALL_CMAKE_DIR}")

  install(
    TARGETS pti pti_view
    EXPORT PtiTargets
    LIBRARY DESTINATION ${PTI_INSTALL_LIB_DIR}
            COMPONENT Pti_Runtime
            NAMELINK_COMPONENT Pti_Development
    ARCHIVE DESTINATION ${PTI_INSTALL_LIB_DIR} COMPONENT Pti_Development
    RUNTIME DESTINATION ${PTI_INSTALL_BIN_DIR} COMPONENT Pti_Runtime)

  install(
    DIRECTORY "${PROJECT_BINARY_DIR}/include/pti"
    DESTINATION "${PTI_INSTALL_INCLUDE_DIR}"
    COMPONENT Pti_Development
    FILES_MATCHING
    PATTERN "*.h")

  install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${PTI_INSTALL_INCLUDE_DIR}"
    COMPONENT Pti_Development
    FILES_MATCHING
    PATTERN "*.h")

  install(
    EXPORT PtiTargets
    FILE PtiTargets.cmake
    NAMESPACE Pti::
    DESTINATION "${PTI_INSTALL_CMAKE_DIR}"
    COMPONENT Pti_Development)

  install(
    FILES "../LICENSE"
    DESTINATION "${PTI_INSTALL_LIC_DIR}"
    RENAME "LICENSE.txt"
    COMPONENT Pti_Doc)

  #
  # This file is only installed in oneAPI drops
  #
  if("${PTI_PRESET_USED}" STREQUAL "oneAPI")
    install(
      FILES "env/vars.sh"
      DESTINATION "env/"
      COMPONENT Pti_oneAPI)
    install(
      FILES "../third-party-programs.txt"
      DESTINATION "${PTI_INSTALL_LIC_DIR}"
      COMPONENT Pti_oneAPI)
  endif()
endif()

if(PTI_BUILD_SAMPLES)
 add_subdirectory(samples/dpc_gemm)
 add_subdirectory(samples/dpc_gemm_threaded)
 add_subdirectory(samples/vector_sq_add)
 add_subdirectory(samples/onemkl_gemm)
 add_subdirectory(samples/iso3dfd_dpcpp)
 add_subdirectory(samples/dlworkloads)
endif()

if(BUILD_TESTING AND PTI_FUZZ)
  add_subdirectory(fuzz)
endif()
