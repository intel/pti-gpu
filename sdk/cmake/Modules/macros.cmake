set(PTI_CMAKE_MACRO_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. CACHE INTERNAL "")

macro(RequirePythonInterp)
  if(NOT DEFINED _Python_EXECUTABLE OR NOT TARGET Python::Interpreter)
    find_package(Python COMPONENTS Interpreter REQUIRED)
    set(PYTHON_EXECUTABLE "${Python_EXECUTABLE}")
  else()
    set(PYTHON_EXECUTABLE "${_Python_EXECUTABLE}")
  endif()
endmacro()

macro(SetPtiVersion)
  file(STRINGS "${PTI_CMAKE_MACRO_DIR}/VERSION" PTI_VERSION)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPTI_VERSION=${PTI_VERSION}")
endmacro()

macro(SetBuildType)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
  endif()
  message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Bitness: 64 bits")
  else()
    message(FATAL_ERROR "32-bit mode is not supported")
  endif()
endmacro()

macro(FindHeadersPath TARGET L0_GEN_SCRIPT GEN_FILE_NAME custom_target L0_TARGET)
  RequirePythonInterp()

  # Use the target that links level zero to find the level zero library
  get_target_property(L0_TARGET_PATH ${L0_TARGET} INTERFACE_INCLUDE_DIRECTORIES)

  # HINTS before PATHS
  find_path(L0_INC_PATH
    NAMES level_zero/ze_api.h
    HINTS ${L0_TARGET_PATH}
    PATHS ENV CPATH)
  if (NOT L0_INC_PATH)
    message(FATAL_ERROR
      "Level Zero headers path is not found.\n"
      "You may need to install oneAPI Level Zero Driver to fix this issue.")
  else()
    message(STATUS "Level Zero headers are found at ${L0_INC_PATH}")
  endif()

  set(L0_GEN_INC_PATH "${PROJECT_BINARY_DIR}")
  if (NOT TARGET unified-runtime::loader)
    find_package(unified-runtime)
  endif()

  # Temporary until we remove UR build dependency.
  set(UR_HEADER_PATH "<empty>")
  if (TARGET unified-runtime::loader)
    get_target_property(UR_HEADER_PATH unified-runtime::loader INTERFACE_INCLUDE_DIRECTORIES)
  endif()

  add_custom_target(${custom_target} ALL
                    DEPENDS ${L0_GEN_INC_PATH}/${GEN_FILE_NAME})
  string(CONCAT PTI_L0_LOADER_COMMIT_INFO "commit: " ${PTI_L0_LOADER_COMMIT_HASH} " - v" ${PTI_L0_LOADER})
  add_custom_command(OUTPUT ${L0_GEN_INC_PATH}/${GEN_FILE_NAME}
                     COMMAND "${PYTHON_EXECUTABLE}" ${L0_GEN_SCRIPT}
                     ${L0_GEN_INC_PATH} "${L0_INC_PATH}/level_zero"
                     "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include/pti" ${UR_HEADER_PATH} ${PTI_API_ID_REGENERATE} ${PTI_L0_LOADER_COMMIT_INFO}
                     DEPENDS ${L0_GEN_SCRIPT})
  target_include_directories(${TARGET}
    PUBLIC "$<BUILD_INTERFACE:${L0_GEN_INC_PATH}>")
  add_dependencies(${TARGET}
    ${custom_target})
endmacro()

macro(CheckIfSyclIsAvailable)
  include(CheckCXXCompilerFlag)
  include(CheckIncludeFileCXX)
  CHECK_CXX_COMPILER_FLAG("-fsycl" HAVE_SYCL)

  if(HAVE_SYCL)
    CHECK_INCLUDE_FILE_CXX("sycl/sycl.hpp" SYCL_IS_AVAILABLE "-fsycl")
    if(NOT SYCL_IS_AVAILABLE)
      set(HAVE_SYCL NO)
    else()
      message(STATUS "Able to compile sycl code. All samples will be built.")
    endif()
  endif()

  if(NOT HAVE_SYCL)
    message(
      STATUS
        "Not able to compile sycl code. Some tests may not run and some samples may not be built."
    )
  endif()
endmacro()

set(SPDLOG_FMT_URL "https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz")
set(SPDLOG_FMT_SHA256 "6cb1e6d37bdcb756dbbe59be438790db409cdb4868c66e888d5df9f13f7c027f")
set(SPDLOG_GABIME_URL "https://github.com/gabime/spdlog/archive/refs/tags/v1.15.0.tar.gz")
set(SPDLOG_GABIME_SHA256 "9962648c9b4f1a7bbc76fd8d9172555bad1871fdb14ff4f842ef87949682caa5")
macro(GetSpdlog)
  if(NOT TARGET spdlog::spdlog)
    find_package(spdlog 1.15.0 QUIET)
  endif()

  if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    if(CMAKE_VERSION VERSION_LESS "3.24")
      FetchContent_Declare(
        fmt
        URL ${SPDLOG_FMT_URL}
        URL_HASH
        SHA256=${SPDLOG_FMT_SHA256}
      )
      FetchContent_Declare(
        spdlog
        URL ${SPDLOG_GABIME_URL}
        URL_HASH
        SHA256=${SPDLOG_GABIME_SHA256}
        )
    else()
      FetchContent_Declare(
        fmt
        URL ${SPDLOG_FMT_URL}
        URL_HASH
        SHA256=${SPDLOG_FMT_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
      )
      FetchContent_Declare(
        spdlog
        URL ${SPDLOG_GABIME_URL}
        URL_HASH
        SHA256=${SPDLOG_GABIME_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
    endif()
    set(FMT_SYSTEM_HEADERS
        ON
        CACHE BOOL "" FORCE)
    set(FMT_INSTALL
        OFF
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED
        OFF
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_PIC
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL_HO
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_SYSTEM_INCLUDES
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_WARNINGS
        OFF
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(fmt spdlog)

    # Prevent fmt from using exceptions because it could throw while logging.
    # Disable warning in fmt due to our usage of EHsc.
    target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
    target_compile_options(fmt PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd6285 $<$<CONFIG:Release>:/wd4702 /wd6385>>)
    target_compile_definitions(fmt-header-only INTERFACE FMT_EXCEPTIONS=0)
    target_compile_options(fmt-header-only INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/wd6285 $<$<CONFIG:Release>:/wd4702 /wd6385>>)

    # spdlog sets the /MP flag on MSVC which causes a warning on icx.
    target_compile_options(spdlog PRIVATE $<$<CXX_COMPILER_ID:IntelLLVM>:-Wno-unused-command-line-argument>)
  endif()
endmacro()

set(GetGTest_URL "https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz")
set(GetGTest_SHA256 "7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926")
macro(GetGTest)
  if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
    include(FetchContent)
    if(CMAKE_VERSION VERSION_LESS "3.24")
      FetchContent_Declare(
        googletest
        URL ${GetGTest_URL}
        URL_HASH
        SHA256=${GetGTest_SHA256}
        )
    else()
      FetchContent_Declare(
        googletest
        URL ${GetGTest_URL}
        URL_HASH
        SHA256=${GetGTest_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
    endif()
    set(INSTALL_GTEST
        OFF
        CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(PTI_GTEST_COMPILE_OPTIONS
      $<$<OR:$<CXX_COMPILER_ID:IntelLLVM>,$<CXX_COMPILER_ID:Clang>>:
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:
          -Wno-deprecated-declarations
          -Wno-character-conversion
          -Wno-unknown-warning-option
        >
      >
      $<$<CXX_COMPILER_ID:MSVC>:/wd6239 /wd6031 /wd6387>
    )

    set_target_properties(gmock_main gmock gtest gtest_main
      PROPERTIES
        COMPILE_OPTIONS "${PTI_GTEST_COMPILE_OPTIONS}")

  endif()
endmacro()

macro(CheckSOVersion PROJ_SOVERSION)
  # Not automatically set because this should be done intentionally.
  # PTI's rules for backwords compatibility should be re-evaluated upon first
  # major / production release.
  # PTI is following [semver](https://semver.org/) versioning and we are using
  # the SOVERSION to denote backword compatibility.
  if ("${PROJECT_VERSION}" VERSION_GREATER_EQUAL "1.0.0")
    if("${PROJ_SOVERSION}" STREQUAL "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
      message(AUTHOR_WARNING "${PROJECT_NAME} currently has the SOVERSION: "
                    "${PROJ_SOVERSION}. After the first major release, ${PROJECT_NAME}"
                    " should have the SOVERSION: ${PROJECT_VERSION_MAJOR}.")
    endif()
  endif()
endmacro()

macro(GetLevelZero PTI_L0_LOADER PTI_L0_LOADER_COMMIT_HASH)
  if (NOT TARGET LevelZero::level-zero)
    # Need zelEnableTracingLayer
    message("-- Fetching L0: ${PTI_L0_LOADER}")
    string(REPLACE "." ";" LZ_TMP_LIST ${PTI_L0_LOADER})
    list(GET LZ_TMP_LIST 0 LZ_VER_MAJOR)
    list(GET LZ_TMP_LIST 1 LZ_VER_MINOR)
    list(GET LZ_TMP_LIST 2 LZ_VER_PATCH)
    set(LZ_VER "${LZ_VER_MAJOR}.${LZ_VER_MINOR}.${LZ_VER_PATCH}")
    set(LZ_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps)

    if(FETCHCONTENT_BASE_DIR)
      set(LZ_BASE_DIR ${FETCHCONTENT_BASE_DIR})
    endif()

    include(FetchContent)
    FetchContent_Declare(
        LevelZero
        GIT_REPOSITORY
        https://github.com/oneapi-src/level-zero.git
        GIT_TAG ${PTI_L0_LOADER_COMMIT_HASH}
    )
    # Prevent content from automatically being installed with PTI
    FetchContent_GetProperties(LevelZero)
    if(NOT LevelZero_POPULATED)
        FetchContent_Populate(LevelZero)
        # Add patch to L0 build
        file(WRITE "${levelzero_SOURCE_DIR}/VERSION_PATCH" ${LZ_VER_PATCH})
        add_subdirectory(${levelzero_SOURCE_DIR} ${levelzero_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    get_target_property(PTI_ZE_LOADER_RUNTIME_DIR ze_loader RUNTIME_OUTPUT_DIRECTORY)

    # Create new target to treat level zero loader as an external dependency.
    # This prevents it from being added to the export set.
    # (Basically treat as if including via find_package)
    add_library(pti_ze_loader INTERFACE IMPORTED)
    add_dependencies(pti_ze_loader ze_tracing_layer ze_validation_layer)

    set(PTI_LZ_COMPILE_OPTIONS
        $<$<CXX_COMPILER_ID:IntelLLVM>:
            -Wno-error
            -Wno-unused-parameter
            -Wno-cast-function-type-mismatch
            -Wno-extra-semi
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /wd6285
            $<$<CONFIG:Release>:/wd4702 /wd6385 /wd6386>
        >
        $<$<CXX_COMPILER_ID:GNU>:
            -Wno-error
            -Wno-unused-parameter
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,8.0.0>:-Wno-extra-semi>
        >
        $<$<CXX_COMPILER_ID:Clang>:
            -Wno-error
            -Wno-unused-parameter
            $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,3.0.0>:-Wno-extra-semi>
        >
    )

    # Silence Warnings from Level Zero Loader. Allows us to better detect PTI
    # warnings and errors.
    set_target_properties(ze_loader ze_tracing_layer ze_null ze_validation_layer
      PROPERTIES
        COMPILE_OPTIONS "${PTI_LZ_COMPILE_OPTIONS}"
        RUNTIME_OUTPUT_DIRECTORY "${PTI_ZE_LOADER_RUNTIME_DIR}/loader")

    # Pull Headers out of source tree and add them to level_zero/
    # This allows us to keep the normal way to include level zero
    # TODO(matthew.schilling@intel.com): Should we move PTI headers to
    # <proj_dir>/include/pti/?
    file(GLOB_RECURSE L0_DL_HEADERS
        LIST_DIRECTORIES TRUE
        "${LZ_BASE_DIR}/levelzero-src/include/*")

    file(COPY ${L0_DL_HEADERS}
        DESTINATION
       ${LZ_BASE_DIR}/levelzero-headers/include/level_zero/)

    # Add new header path to our new target
    find_path(LZ_INCLUDE_DIR
      NAMES level_zero/ze_api.h
      HINTS ${LZ_BASE_DIR}/levelzero-headers
      PATH_SUFFIXES include
      NO_PACKAGE_ROOT_PATH
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )

    set_target_properties(pti_ze_loader PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES ${LZ_INCLUDE_DIR})

    target_link_libraries(pti_ze_loader INTERFACE
                            $<BUILD_INTERFACE:ze_loader>)
    add_library(LevelZero::level-zero ALIAS pti_ze_loader)
    add_library(LevelZero::headers INTERFACE IMPORTED)
    set_target_properties(
      LevelZero::headers
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LZ_INCLUDE_DIR}")
  endif()
endmacro()

macro(RemoveNDebugFlag)
  if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM" OR CMAKE_CXX_COMPILER_ID MATCHES
                                                  "Clang")
    # One must make sure -DNDEBUG is not set if using XPTI filename/linenumber
    # https://stackoverflow.com/questions/22140520/how-to-enable-assert-in-cmake-release-mode
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE
                   "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO
                   "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endif()
endmacro()

macro(ProjectIsTopLevel)
  if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    set(PROJECT_IS_TOP_LEVEL FALSE)
    get_property(PTI_PROJ_PARENT DIRECTORY PROPERTY PARENT_DIRECTORY)
    if(PTI_PROJ_PARENT STREQUAL "")
      set(PROJECT_IS_TOP_LEVEL TRUE)
    endif()
  endif()
endmacro()

macro(AddApiGenTarget L0_GEN_SCRIPT GEN_FILE_NAME L0_TARGET)
  RequirePythonInterp()

  # Use the target that links level zero to find the level zero library
  if(TARGET LevelZero::level-zero)
    get_target_property(L0_TARGET_PATH ${L0_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    include(CMakePrintHelpers)
    cmake_print_variables(PTI_L0_LOADER PTI_L0_LOADER_COMMIT_HASH L0_TARGET_PATH)
  endif()

  # HINTS before PATHS
  find_path(L0_INC_PATH
    NAMES level_zero/ze_api.h
    HINTS ${L0_TARGET_PATH}
    PATHS ENV CPATH)
  if (NOT L0_INC_PATH)
    message(FATAL_ERROR
      "Level Zero headers path is not found.\n"
      "You may need to install oneAPI Level Zero Driver to fix this issue.")
  else()
    message(STATUS "Level Zero headers are found at ${L0_INC_PATH}")
  endif()

  set(L0_GEN_INC_PATH "${PROJECT_BINARY_DIR}")
  if (NOT TARGET unified-runtime::loader)
    find_package(unified-runtime)
  endif()

  # Temporary until we remove UR build dependency.
  set(UR_HEADER_PATH "<empty>")
  if (TARGET unified-runtime::loader)
    get_target_property(UR_HEADER_PATH unified-runtime::loader INTERFACE_INCLUDE_DIRECTORIES)
  endif()

  string(CONCAT L0_LOADER_INFO "commit: " ${PTI_L0_LOADER_COMMIT_HASH} " - v" ${PTI_L0_LOADER})
  add_custom_target(generate-ids
                    DEPENDS ${L0_GEN_INC_PATH}/${GEN_FILE_NAME}
                    COMMAND "${PYTHON_EXECUTABLE}" ${L0_GEN_SCRIPT}
                    ${L0_GEN_INC_PATH} "${L0_INC_PATH}/level_zero"
                    "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include/pti" ${UR_HEADER_PATH} "ON" ${L0_LOADER_INFO})
endmacro()

macro(AddFormatTarget)
  find_program(CLANG_FORMAT_EXE clang-format)
  find_program(BLACK_FORMAT_EXE black)

  add_custom_target(format)
  add_custom_target(format-chk)

  cmake_policy(PUSH)
  cmake_policy(SET CMP0009 NEW)
  file(GLOB_RECURSE cf_src_files  "${PROJECT_SOURCE_DIR}/src/*.cc"
                                  "${PROJECT_SOURCE_DIR}/src/*.h"
                                  "${PROJECT_SOURCE_DIR}/src/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/src/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/test/*.cc"
                                  "${PROJECT_SOURCE_DIR}/test/*.h"
                                  "${PROJECT_SOURCE_DIR}/test/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/test/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/samples/*.cc"
                                  "${PROJECT_SOURCE_DIR}/samples/*.h"
                                  "${PROJECT_SOURCE_DIR}/samples/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/samples/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/fuzz/*.cc"
                                  "${PROJECT_SOURCE_DIR}/fuzz/*.h"
                                  "${PROJECT_SOURCE_DIR}/fuzz/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/fuzz/**/*.cc")
  list(FILTER cf_src_files EXCLUDE REGEX
                                  "^${PROJECT_SOURCE_DIR}/samples/dlworkload*")
  list(FILTER cf_src_files EXCLUDE REGEX
                                   "^${PROJECT_SOURCE_DIR}/samples/iso3d*")
  list(FILTER cf_src_files EXCLUDE REGEX
                                  "^${PROJECT_SOURCE_DIR}/build*")

  add_custom_target(format-cpp COMMAND ${CLANG_FORMAT_EXE} -i ${cf_src_files})
  add_custom_target(format-cpp-chk COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${cf_src_files})

  add_dependencies(format format-cpp)
  add_dependencies(format-chk format-cpp-chk)

  if(BLACK_FORMAT_EXE)
    list(APPEND py_src_dirs "${PROJECT_SOURCE_DIR}/cmake"
                            "${PROJECT_SOURCE_DIR}/src"
                            "${PROJECT_SOURCE_DIR}/fuzz"
                            "${PROJECT_SOURCE_DIR}/test"
                            "${PROJECT_SOURCE_DIR}/samples")
    file(GLOB py_src_dirs_cur "${PROJECT_SOURCE_DIR}/*.py")
    set(py_src_dirs ${py_src_dirs} ${py_src_dirs_cur})
    add_custom_target(format-py COMMAND ${BLACK_FORMAT_EXE} ${py_src_dirs})
    add_custom_target(format-py-chk COMMAND ${BLACK_FORMAT_EXE} --check ${py_src_dirs})

    add_dependencies(format format-py)
    add_dependencies(format-chk format-py-chk)
  else()
    message(STATUS "black not found. Python code cannot be formatted.")
  endif()
  cmake_policy(POP)
endmacro()

macro(AddProjectVersionInfo TARGET)
  if (WIN32)
    set(PTI_VERSIONINFO_RC "${PROJECT_BINARY_DIR}/${TARGET}_versioninfo.rc")
    set(PTI_VERSIONINFO_RC_IN "${PROJECT_SOURCE_DIR}/cmake/Modules/pti_versioninfo.rc.in")

    set(INTEL_LIC_PATH "${PTI_CMAKE_MACRO_DIR}/../LICENSE")

    if (EXISTS "${INTEL_LIC_PATH}")
      file(STRINGS "${INTEL_LIC_PATH}" INTEL_LICENSE_FILE_STRINGS)
      foreach(PTI_STRING ${INTEL_LICENSE_FILE_STRINGS})
        string(REGEX MATCH "^Copyright.*" PTI_COPYRIGHT "${PTI_STRING}")
        if (PTI_COPYRIGHT)
          break()
        endif()
      endforeach()
    endif()

    if(NOT PTI_COPYRIGHT)
      message(WARNING "Copyright file not found, Windows versioninfo will be incomplete")
      set(PTI_COPYRIGHT "")
    endif()

    configure_file(${PTI_VERSIONINFO_RC_IN} ${PTI_VERSIONINFO_RC} @ONLY)
    target_sources(${TARGET} PRIVATE ${PTI_VERSIONINFO_RC})
  endif()
endmacro()

#
# Check whether to use experimental/filesystem or filesystem.
#
# defines `PTI_EXPERIMENTAL_FILESYSTEM` if the platform does not
# support std::filesystem usage.
# sets `FS_LIB` with proper library to link (empty if not needed).
#
macro(CheckExperimentalFilesystem)
  if(NOT DEFINED PTI_EXPERIMENTAL_FILESYSTEM AND NOT DEFINED FS_LIB)
    set(FS_LIB "")
    cmake_policy(PUSH)
    cmake_policy(SET CMP0067 NEW)
    include(CMakePushCheckState)
    cmake_push_check_state()
    cmake_reset_check_state()
    include(CheckCXXSourceCompiles)
    # Check normal case
    set(code
      "#include <filesystem>
      int main()
      {
        return std::filesystem::exists(std::filesystem::temp_directory_path());
      }")
    set(CMAKE_REQUIRED_LIBRARIES ${FS_LIB})
    check_cxx_source_compiles("${code}" COMPILES_STD_FILESYSTEM)

    if(NOT COMPILES_STD_FILESYSTEM)
      cmake_reset_check_state()

      # Check experimental filesystem case
      set(code
        "#include <experimental/filesystem>
        int main()
        {
          return std::experimental::filesystem::exists(std::experimental::filesystem::temp_directory_path());
        }")
      set(CMAKE_REQUIRED_LIBRARIES ${FS_LIB})
      check_cxx_source_compiles("${code}" COMPILES_STD_EXPERIMENTAL_FILESYSTEM)
    endif()

    # Check experimental filesystem glib case
    if(NOT COMPILES_STD_FILESYSTEM AND NOT COMPILES_STD_EXPERIMENTAL_FILESYSTEM)
      cmake_reset_check_state()
      set(FS_LIB stdc++fs)
      set(CMAKE_REQUIRED_LIBRARIES ${FS_LIB})
      check_cxx_source_compiles("${code}"
        COMPILES_STD_EXPERIMENTAL_FILESYSTEM_LIBSTDCXXFS)
      if(COMPILES_STD_EXPERIMENTAL_FILESYSTEM_LIBSTDCXXFS)
        set(COMPILES_STD_EXPERIMENTAL_FILESYSTEM TRUE)
      endif()
    endif()

    # Check experimental filesystem LLVM case
    if(NOT COMPILES_STD_FILESYSTEM AND NOT COMPILES_STD_EXPERIMENTAL_FILESYSTEM_LIBSTDCXXFS)
      cmake_reset_check_state()
      set(FS_LIB c++fs)
      set(CMAKE_REQUIRED_LIBRARIES ${FS_LIB})
      check_cxx_source_compiles("${code}"
        COMPILES_STD_EXPERIMENTAL_FILESYSTEM_LIBCXXFS)
      if(COMPILES_STD_EXPERIMENTAL_FILESYSTEM_LIBCXXFS)
        set(COMPILES_STD_EXPERIMENTAL_FILESYSTEM TRUE)
      endif()
    endif()

    cmake_pop_check_state()
    cmake_policy(POP)

    if(NOT COMPILES_STD_FILESYSTEM)
      if(NOT COMPILES_STD_EXPERIMENTAL_FILESYSTEM)
        message(FATAL_ERROR "C++17 filesystem support is required")
      endif()
      set(PTI_EXPERIMENTAL_FILESYSTEM ON)
    else()
      set(FS_LIB "") # Clear if we have std::filesystem
      unset(PTI_EXPERIMENTAL_FILESYSTEM)
    endif()
  endif()
endmacro()

macro(AddVersionlessLinkFile MY_TARGET)
  add_custom_command(TARGET ${MY_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:${MY_TARGET}>
    $<TARGET_LINKER_FILE_DIR:${MY_TARGET}>/${CMAKE_SHARED_LIBRARY_PREFIX}${MY_TARGET}$<$<PLATFORM_ID:Windows>:$<$<CONFIG:Debug>:d>>${CMAKE_LINK_LIBRARY_SUFFIX}
  )
endmacro()
