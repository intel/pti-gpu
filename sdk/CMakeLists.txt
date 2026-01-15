set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(macros)
cmake_minimum_required(VERSION 3.14)

SetPtiVersion()

project(
  pti
  VERSION ${PTI_VERSION}
  DESCRIPTION "Intel Profiling Tools Interface"
  HOMEPAGE_URL "https://github.com/intel/pti-gpu"
  LANGUAGES C CXX
)

ProjectIsTopLevel()
SetBuildType()

# Global Settings
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin CACHE STRING "")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib CACHE STRING "")
endif()

# For developer use with tools like clangd(can be used by vscode and vim
# plugins).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(PROJECT_IS_TOP_LEVEL)
  AddFormatTarget()
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
else()
  if(CMAKE_CXX_STANDARD LESS 17 OR CMAKE_CXX_STANDARD EQUAL 98)
    message(FATAL_ERROR "CMAKE_CXX_STANDARD must be at least 17")
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET)
  set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
endif()

if(NOT DEFINED CMAKE_C_STANDARD)
  # C99 for the C headers in the PTI interface (${PROJECT_SOURCE_DIR}/include).
  # They include stdint.h.
  set(CMAKE_C_STANDARD 99)
endif()

if(NOT DEFINED CMAKE_C_STANDARD_REQUIRED)
  set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

if(NOT DEFINED CMAKE_C_EXTENSIONS)
  set(CMAKE_C_EXTENSIONS OFF)
endif()

if(NOT DEFINED CMAKE_C_VISIBILITY_PRESET)
  set(CMAKE_C_VISIBILITY_PRESET "hidden")
endif()

if(NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

if(PROJECT_IS_TOP_LEVEL)
  # Add the current directory to the rpath during the build. On Linux, this
  # allows dlopen to find the proper modules.
  if(NOT APPLE)
    list(APPEND CMAKE_BUILD_RPATH $ORIGIN)
  endif()
endif()

option(PTI_BUILD_TESTING "Build PTI tests" ${PROJECT_IS_TOP_LEVEL})
option(PTI_BUILD_SAMPLES "Build samples with library" ${PROJECT_IS_TOP_LEVEL})
option(PTI_INSTALL "Install library when invoking --install" ON)
option(PTI_ENABLE_LOGGING "Enable logging for Pti" OFF)
option(PTI_FUZZ "Enable Fuzz Pti" OFF)
option(PTI_API_ID_REGENERATE "Regenerate Apiid files" OFF)

include(GNUInstallDirs)
set(PTI_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pti")
set(PTI_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(PTI_INSTALL_LIB_DIR "${CMAKE_INSTALL_LIBDIR}")
set(PTI_INSTALL_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
set(PTI_INSTALL_LIC_DIR "${CMAKE_INSTALL_DOCDIR}")
set(PTI_API_SOVERSION "${PROJECT_VERSION_MAJOR}")

CheckSOVersion(${PTI_API_SOVERSION})
CheckExperimentalFilesystem()

# Check and Get Dependencies
GetSpdlog()

find_package(Xpti)

set(PTI_L0_LOADER 1.26.3)
set(PTI_L0_LOADER_COMMIT_HASH "354190cd2e7aa485b55a39c91d108413a94cc5e1")

if(NOT PROJECT_IS_TOP_LEVEL)
  find_package(LevelZero ${PTI_L0_LOADER} REQUIRED)
else()
  find_package(LevelZero ${PTI_L0_LOADER})
endif()

if(NOT TARGET LevelZero::level-zero)
  GetLevelZero(${PTI_L0_LOADER} ${PTI_L0_LOADER_COMMIT_HASH})
endif()

find_package(Threads REQUIRED)

# Build PTI libraries.
configure_file(${PROJECT_SOURCE_DIR}/include/pti/pti_version.h.in
               ${PROJECT_BINARY_DIR}/include/pti/pti_version.h)

# Generate file with configuration information above.
configure_file(${PROJECT_SOURCE_DIR}/src/utils/platform_config.h.in
               ${PROJECT_BINARY_DIR}/platform_config.h
               @ONLY)

# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
add_library(pti_all INTERFACE)
add_library(Pti::pti_all ALIAS pti_all)

# Core implementation loaded by pti_view
add_library(pti MODULE "${PROJECT_SOURCE_DIR}/src/pti_view.cc"
                       "${PROJECT_SOURCE_DIR}/src/levelzero/ze_driver_init.cc"
                       "${PROJECT_SOURCE_DIR}/src/pti_callbacks.cc"
                       "${PROJECT_SOURCE_DIR}/src/metrics/pti_metrics.cc"
                       "${PROJECT_SOURCE_DIR}/src/metrics/pti_metrics_scope.cc"
                       "${PROJECT_SOURCE_DIR}/src/pti_version.cc")
# We might want to make our own, but for now use CMake's
# https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html
include(GenerateExportHeader)
generate_export_header(pti EXPORT_FILE_NAME
                 ${PROJECT_BINARY_DIR}/include/pti/pti_export.h)

# Suspect we need this since we're sharing across multiple targets
set(PTI_DEFINE_SYMBOL "pti_EXPORTS")

add_library(Pti::pti ALIAS pti)

# Where to find headers that user will include.
set(PTI_PUBLIC_INCLUDE_DIRS
  "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${PTI_INSTALL_INCLUDE_DIR}>"
)

set(PTI_PRIVATE_INCLUDE_DIRS
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/sycl"
  "${PROJECT_SOURCE_DIR}/src/levelzero"
  "${PROJECT_SOURCE_DIR}/src/utils"
)

target_include_directories(
  pti
  PUBLIC ${PTI_PUBLIC_INCLUDE_DIRS}
  PRIVATE ${PTI_PRIVATE_INCLUDE_DIRS})

set_target_properties(
  pti
  PROPERTIES VERSION "${PROJECT_VERSION}"
             SOVERSION "${PTI_API_SOVERSION}"
             POSITION_INDEPENDENT_CODE ON)

if (WIN32)
  set_target_properties(
    pti
    PROPERTIES DEBUG_POSTFIX "d"
  )
  AddProjectVersionInfo(pti)
endif()

target_compile_definitions(
  pti
  PRIVATE $<$<TARGET_EXISTS:Xpti::xpti>:PTI_TRACE_SYCL>
          $<$<TARGET_EXISTS:LevelZero::level-zero>:PTI_LEVEL_ZERO=1>
          $<$<CONFIG:Debug>:PTI_XPTI_FRAMEWORK_DISPATCHER_NAME=$<TARGET_FILE_NAME:Xpti::xpti>>
          $<IF:$<BOOL:${PTI_ENABLE_LOGGING}>,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)

target_link_libraries(
  pti
  PUBLIC Threads::Threads
  PRIVATE $<TARGET_NAME_IF_EXISTS:Xpti::xpti>
          LevelZero::level-zero
          spdlog::spdlog
          ${CMAKE_DL_LIBS}
          ${FS_LIB})

FindHeadersPath(
  pti "${PROJECT_SOURCE_DIR}/src/gen_tracing_callbacks.py"
  "tracing.gen" gen_tracing_header LevelZero::level-zero)

if(PROJECT_IS_TOP_LEVEL)
  AddApiGenTarget(
    "${PROJECT_SOURCE_DIR}/src/gen_tracing_callbacks.py" "tracing.gen" LevelZero::level-zero)
  add_dependencies(generate-ids gen_tracing_header)
endif()

# Library that loads core (pti). User links to this one.
add_library(pti_view SHARED "${PROJECT_SOURCE_DIR}/src/xpti_adapter.cc"
                            "${PROJECT_SOURCE_DIR}/src/pti.cc"
                            "${PROJECT_SOURCE_DIR}/src/pti_version.cc"
                            "${PROJECT_SOURCE_DIR}/src/pti_view_load.cc")

add_library(Pti::pti_view ALIAS pti_view)

target_include_directories(
  pti_view
  PUBLIC ${PTI_PUBLIC_INCLUDE_DIRS}
  PRIVATE ${PTI_PRIVATE_INCLUDE_DIRS})

set_target_properties(
  pti_view
  PROPERTIES VERSION "${PROJECT_VERSION}"
             SOVERSION "${PTI_API_SOVERSION}"
             POSITION_INDEPENDENT_CODE ON
	           DEFINE_SYMBOL "${PTI_DEFINE_SYMBOL}")

if (WIN32)
  set_target_properties(
    pti_view
    PROPERTIES OUTPUT_NAME "pti_view-${PROJECT_VERSION_MAJOR}"
               DEBUG_POSTFIX "d"
  )

  AddVersionlessLinkFile(pti_view)

  AddProjectVersionInfo(pti_view)
endif()

target_compile_definitions(
  pti_view
  PRIVATE PTI_VIEW_CORE_LIB_NAME=$<TARGET_FILE_NAME:pti>
          $<$<CONFIG:Debug>:PTI_XPTI_FRAMEWORK_DISPATCHER_NAME=$<TARGET_FILE_NAME:Xpti::xpti>>
          $<IF:$<BOOL:${PTI_ENABLE_LOGGING}>,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,
          SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)

target_link_libraries(
  pti_view
  PRIVATE spdlog::spdlog
          ${CMAKE_DL_LIBS}
          ${FS_LIB})

target_link_libraries(
	pti_all INTERFACE Pti::pti_view)

include(CTest)

if(BUILD_TESTING AND PTI_BUILD_SAMPLES)
  # https://github.com/uxlfoundation/oneTBB/commit/d784b628190cbbcb6139916da3efe8c0b60abaa2
  set(TBB_FIND_RELEASE_ONLY ON CACHE BOOL
    "Only use release TBB to workaround crash with tbb_debug")
  add_subdirectory(samples/dlworkloads)
  add_subdirectory(samples/dpc_gemm)
  add_subdirectory(samples/dpc_gemm_threaded)
  add_subdirectory(samples/vector_sq_add)
  add_subdirectory(samples/onemkl_gemm)
  add_subdirectory(samples/iso3dfd_dpcpp)
  add_subdirectory(samples/metrics_iso3dfd_dpcpp)
  add_subdirectory(samples/callback)
  add_subdirectory(samples/metrics_scope)
  add_subdirectory(samples/omp_vec_add)
  add_subdirectory(samples/metrics_perf)
endif()

if(BUILD_TESTING AND PTI_BUILD_TESTING)
  add_subdirectory(test)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(cmake/coverage.cmake)
endif()

# Targets / Install phase
export(
  TARGETS pti_all pti pti_view
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
    TARGETS pti_all pti pti_view
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

  if(WIN32)
    install(
      FILES $<TARGET_LINKER_FILE_DIR:pti_view>/pti_view$<$<CONFIG:Debug>:d>${CMAKE_LINK_LIBRARY_SUFFIX}
      COMPONENT Pti_Development
      DESTINATION "${PTI_INSTALL_LIB_DIR}")
  endif()

  if (EXISTS "${PROJECT_SOURCE_DIR}/../LICENSE")
    install(
      FILES "../LICENSE"
      DESTINATION "${PTI_INSTALL_LIC_DIR}"
      RENAME "LICENSE.txt"
      COMPONENT Pti_Doc)
  endif()

  #
  # This file is only installed in oneAPI drops
  #
  if("${PTI_PRESET_USED}" MATCHES "[lw].*-oneAPI-release")

    include(cmake/bom_macro.cmake)

    set(BOMFILE_DIR "drop_raw")
    set(BOMFILE_DEV "${BOMFILE_DIR}/pti_dev.txt")
    set(BOMFILE_RUN "${BOMFILE_DIR}/pti_run.txt")

    install(CODE "
      file(REMOVE ${BOMFILE_DEV}  ${BOMFILE_RUN}) "
      COMPONENT Pti_Bom)

    if (EXISTS "${PROJECT_SOURCE_DIR}/../third-party-programs.txt")
      install(
        FILES "../third-party-programs.txt"
        DESTINATION "${PTI_INSTALL_LIC_DIR}"
        COMPONENT Pti_oneAPI)
    else()
      message(FATAL_ERROR "The PTI library must be built within the top-level "
        "repository to ensure a valid installation.")
    endif()

    if (UNIX)
      install(FILES "env/vars.sh" DESTINATION "env/" COMPONENT Pti_oneAPI)
      install(FILES "env/oneapi-vars.sh" DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/pti" RENAME vars.sh COMPONENT Pti_oneAPI)
      install(DIRECTORY "cmake/tags/" DESTINATION "tags/" FILES_MATCHING PATTERN "lin_*.txt")
    elseif (WIN32)
      install(FILES "env/vars.bat" DESTINATION "env/" COMPONENT Pti_oneAPI)
      install(FILES "env/oneapi-vars.bat" DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/pti" RENAME vars.bat COMPONENT Pti_oneAPI)
      install(DIRECTORY "cmake/tags/" DESTINATION "tags/" FILES_MATCHING PATTERN "win_*.txt")
    endif()

    set(BOMFILE_DIRNULL "NULL")
    set(BOMFILE_DIRPREFIX "-LIB")

    bomline(${BOMFILE_DIR} ${BOMFILE_DEV} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_INCLUDE_DIR}/pti*.h\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_DEV} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIB_DIR}/cmake/pti/Pti*.cmake\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIC_DIR}/*\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIB_DIR}/libpti*so.[0-9]*\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIB_DIR}/libpti*so\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_DEV} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIB_DIR}/*.lib\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRPREFIX}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_LIB_DIR}/*.dll\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRPREFIX}
      [[\"\${CMAKE_INSTALL_PREFIX}/${PTI_INSTALL_BIN_DIR}/*.dll\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/env/*\"]])
    bomline(${BOMFILE_DIR} ${BOMFILE_RUN} ${BOMFILE_DIRNULL}
      [[\"\${CMAKE_INSTALL_PREFIX}/etc/*\"]])

 endif()
endif()

if(PROJECT_IS_TOP_LEVEL AND PTI_INSTALL)
  include(cmake/packaging.cmake)
endif()
