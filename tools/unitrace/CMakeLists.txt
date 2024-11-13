include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Tools_UniTrace C CXX)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
SetCompilerFlags()
SetBuildType()

if(WIN32)
  #Win_Todo :  Default MPI is disabled on windows.It will be enabled once verified.
  option(BUILD_WITH_MPI
  "Build with support for MPI tracing"
  OFF
  )
else()
  option(BUILD_WITH_MPI
    "Build with support for MPI tracing"
    ON
  )
endif()

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 ")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fvisibility=default")
endif()

# Tool Library
macro(FindHeadersPath TARGET L0_GEN_SCRIPT GEN_FILE_NAME custom_target)
  RequirePythonInterp()
  find_path(L0_INC_PATH
    NAMES level_zero
    PATHS ENV CPATH)
  if (NOT L0_INC_PATH)
    message(FATAL_ERROR
      "Level Zero headers path is not found.\n"
      "You may need to install oneAPI Level Zero Driver to fix this issue.")
  else()
    message(STATUS "Level Zero headers are found at ${L0_INC_PATH}")
  endif()

  set(L0_GEN_INC_PATH "${CMAKE_BINARY_DIR}")
  set(OCL_DIR_PATH "${CMAKE_BINARY_DIR}/CL")
  add_custom_target(${custom_target} ALL
                    DEPENDS ${L0_GEN_INC_PATH}/${GEN_FILE_NAME})
  add_custom_command(OUTPUT ${L0_GEN_INC_PATH}/${GEN_FILE_NAME}
                     COMMAND "${PYTHON_EXECUTABLE}" ${L0_GEN_SCRIPT} ${L0_GEN_INC_PATH} "${L0_INC_PATH}/level_zero" "${OCL_DIR_PATH}")
  target_include_directories(${TARGET}
    PUBLIC "${L0_GEN_INC_PATH}")
  add_dependencies(${TARGET}
    ${custom_target})
endmacro()

# Get Git Commit Hash
macro(GetGitCommitHash TARGET GET_COMMIT_HASH_SCRIPT COMMIT_HASH_FILE_NAME target)
  RequirePythonInterp()

  add_custom_target(${target} ALL DEPENDS ${CMAKE_BINARY_DIR}/${COMMIT_HASH_FILE_NAME})
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${COMMIT_HASH_FILE_NAME}
                     COMMAND "${PYTHON_EXECUTABLE}" ${GET_COMMIT_HASH_SCRIPT} ${CMAKE_BINARY_DIR} ${COMMIT_HASH_FILE_NAME})
  add_dependencies(${TARGET} ${target})
endmacro()

find_package(PythonInterp 3.9 REQUIRED)

# Download itt source from github repo
set(ITT_HASH 47467459a7984988ab838b5108c03ab1cf0a3f73)
execute_process(COMMAND "${PYTHON_EXECUTABLE}" ${PROJECT_SOURCE_DIR}/scripts/get_itt.py ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR} ${ITT_HASH})

add_subdirectory(${CMAKE_BINARY_DIR}/ittapi)
if (BUILD_WITH_MPI)
  find_package(MPI REQUIRED)
  add_library(unitrace_mpi SHARED
    "${PROJECT_SOURCE_DIR}/src/mpi/mpi.c")
  add_dependencies(unitrace_mpi ittnotify)

  target_link_libraries(unitrace_mpi
    PUBLIC MPI::MPI_C
    PRIVATE ${CMAKE_BINARY_DIR}/bin/libittnotify.a)
  target_include_directories(unitrace_mpi
    PRIVATE "${PROJECT_SOURCE_DIR}"
    PRIVATE "${I_MPI_ROOT}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
    PRIVATE "${PROJECT_SOURCE_DIR}/../../utils"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/itt"
    PRIVATE "${CMAKE_BINARY_DIR}/ittheaders"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/mpi"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/opencl"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/levelzero")
  if(CMAKE_INCLUDE_PATH)
    target_include_directories(unitrace_mpi
      PUBLIC "${CMAKE_INCLUDE_PATH}")
  endif()
endif()

add_library(unitrace_tool SHARED
  "${PROJECT_SOURCE_DIR}/src/opencl/cl_ext_collector.cc"
  "${PROJECT_SOURCE_DIR}/../../utils/trace_guard.cc"
  "${PROJECT_SOURCE_DIR}/src/tracer.cc")
link_directories(${ONEAPI_COMPILER_HOME}/lib)
find_package(Xptifw REQUIRED)

if (Xptifw_FOUND)
  target_include_directories(unitrace_tool PRIVATE "${Xptifw_INCLUDE_DIR}")
  if(WIN32)
    target_link_libraries(unitrace_tool PRIVATE ${Xptifw_LIBRARY} "${CMAKE_BINARY_DIR}/ze_loader.lib")
  else()
    target_link_libraries(unitrace_tool PRIVATE xptifw)
  endif()
endif()

target_include_directories(unitrace_tool
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${ONEAPI_COMPILER_HOME}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/xpti"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/itt"
  PRIVATE "${CMAKE_BINARY_DIR}/ittheaders"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/opencl"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/levelzero")

target_compile_definitions(unitrace_tool PUBLIC PTI_LEVEL_ZERO=1)
if(CMAKE_INCLUDE_PATH)
  target_include_directories(unitrace_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(unitrace_tool)
FindOpenCLHeaders(unitrace_tool)

GetOpenCLTracingHeaders(unitrace_tool)

FindL0Library(unitrace_tool)
FindL0Headers(unitrace_tool)

FindHeadersPath(unitrace_tool "${PROJECT_SOURCE_DIR}/scripts/gen_tracing_common_header.py" "common_header.gen" gen_common_header)
FindHeadersPath(unitrace_tool "${PROJECT_SOURCE_DIR}/scripts/gen_tracing_callbacks.py" "tracing.gen" gen_tracing_header)

add_dependencies(gen_common_header
    cl_tracing_headers)
add_dependencies(gen_tracing_header
    cl_tracing_headers)

if(WIN32)
  add_dependencies(gen_common_header
    level_zero_headers)
  add_dependencies(gen_tracing_header
    level_zero_headers)
endif()

target_include_directories(unitrace_tool PRIVATE "${L0_INC_PATH}")

# Loader

add_executable(unitrace "${PROJECT_SOURCE_DIR}/src/unitrace.cc")

if(UNIX)
    include(CMakePushCheckState)
    cmake_push_check_state()
    cmake_reset_check_state()
    include(CheckCXXSourceCompiles)
    string(CONFIGURE [[
        #include <filesystem>
        int main()
        {
            if (!std::filesystem::exists(std::filesystem::path("/tmp"))) {
                return 0;
            }
            return 1;
        }
        ]] code @ONLY)
    check_cxx_source_compiles("${code}" success)
    if(success)
        set(CXX_STD_FILESYSTEM_EXPERIMENTAL 0)
    else()
        string(CONFIGURE [[
        #include <experimental/filesystem>
        int main()
        {
            if (!std::experimental::filesystem::exists(std::experimental::filesystem::path("/tmp"))) {
                return 0;
            }
            return 1;
        }
        ]] code @ONLY)
        set(saved_libs ${CMAKE_REQUIRED_LIBRARIES})
        set(CMAKE_REQUIRED_LIBRARIES ${saved_libs} "-lstdc++fs")
        check_cxx_source_compiles("${code}" success)
        if (success)
            set(CXX_STD_FILESYSTEM_EXPERIMENTAL 1)
            target_link_libraries(unitrace stdc++fs)
        else()
            set(CMAKE_REQUIRED_LIBRARIES ${saved_libs} "-lc++fs")
            check_cxx_source_compiles("${code}" success)
            if (success)
                set(CXX_STD_FILESYSTEM_EXPERIMENTAL 1)
                target_link_libraries(unitrace c++fs)
            else()
                message(FATAL_ERROR "C++17 filesystem support is required")
            endif()
        endif()
    endif()
    cmake_pop_check_state()
    target_compile_definitions(unitrace PRIVATE CXX_STD_FILESYSTEM_EXPERIMENTAL=${CXX_STD_FILESYSTEM_EXPERIMENTAL})
endif()

set(_use_mpi 1)

if (BUILD_WITH_MPI)
  set(_use_mpi 1)
else()
  set(_use_mpi 0)
endif()
target_compile_definitions(unitrace PRIVATE BUILD_WITH_MPI=${_use_mpi})
target_include_directories(unitrace
  PRIVATE "${CMAKE_BINARY_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/levelzero"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(unitrace pthread dl)
endif()

FindL0Library(unitrace)
FindL0Headers(unitrace)
GetGitCommitHash(unitrace "${PROJECT_SOURCE_DIR}/scripts/get_commit_hash.py" "unitrace_commit_hash.h" get_git_commit_hash_unitrace)
GetGitCommitHash(unitrace_tool "${PROJECT_SOURCE_DIR}/scripts/get_commit_hash.py" "unitrace_tool_commit_hash.h" get_git_commit_hash_unitrace_tool)
if(WIN32)
  target_link_libraries(unitrace "${CMAKE_BINARY_DIR}/ze_loader.lib")
endif()

if(UNIX)
  add_custom_command(TARGET unitrace POST_BUILD COMMAND "rm" "-rf" "unitrace_commit_hash.h")
  add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "rm" "-rf" "unitrace_tool_commit_hash.h")
else()
  add_custom_command(TARGET unitrace POST_BUILD COMMAND "del" "unitrace_commit_hash.h")
  add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "del" "unitrace_tool_commit_hash.h")
endif()

# Clearning files only for release build, for any other build types lets skip deletion for better debuggability
string(TOLOWER "${CMAKE_BUILD_TYPE}" LOWER_CMAKE_BUILD_TYPE)
if(NOT LOWER_CMAKE_BUILD_TYPE STREQUAL "debug")
  if(UNIX)
    add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "rm" "-rf" "tracing.gen")
    add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "rm" "-rf" "common_header.gen")
  else()
    add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "del" "tracing.gen")
    add_custom_command(TARGET unitrace_tool POST_BUILD COMMAND "del" "common_header.gen")
  endif()
endif()

configure_file(${PROJECT_SOURCE_DIR}/scripts/mergetrace.py ${CMAKE_BINARY_DIR}/scripts/mergetrace.py COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/scripts/analyzeperfmetrics.py ${CMAKE_BINARY_DIR}/scripts/analyzeperfmetrics.py COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/scripts/addrasm.py ${CMAKE_BINARY_DIR}/scripts/addrasm.py COPYONLY)

# Installation

install(TARGETS unitrace unitrace_tool DESTINATION bin)
if (BUILD_WITH_MPI)
  install(TARGETS unitrace_mpi DESTINATION bin)
endif()
