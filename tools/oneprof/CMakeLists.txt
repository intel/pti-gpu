include("../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Tools_OneProf CXX)
SetCompilerFlags()
SetBuildType()

# Tool Library

add_library(oneprof_tool SHARED
  "${PROJECT_SOURCE_DIR}/../../loader/init.cc"
  "${PROJECT_SOURCE_DIR}/../utils/correlator.cc"
  "${PROJECT_SOURCE_DIR}/../../utils/trace_guard.cc"
  tool.cc)
target_include_directories(oneprof_tool
  PRIVATE "${PROJECT_SOURCE_DIR}"
  PRIVATE "${PROJECT_SOURCE_DIR}/../ze_tracer"
  PRIVATE "${PROJECT_SOURCE_DIR}/../cl_tracer"
  PRIVATE "${PROJECT_SOURCE_DIR}/../utils"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
target_compile_definitions(oneprof_tool PUBLIC PTI_KERNEL_INTERVALS=1)
target_compile_definitions(oneprof_tool PUBLIC PTI_LEVEL_ZERO=1)
if(DEFINED PTI_OA_TIMESTAMP_VALID_BITS)
  message(STATUS "PTI_OA_TIMESTAMP_VALID_BITS is set to ${PTI_OA_TIMESTAMP_VALID_BITS}")
  target_compile_definitions(oneprof_tool PUBLIC PTI_OA_TIMESTAMP_VALID_BITS=${PTI_OA_TIMESTAMP_VALID_BITS})
endif()
if(CMAKE_INCLUDE_PATH)
  target_include_directories(oneprof_tool
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()

FindOpenCLLibrary(oneprof_tool)
FindOpenCLHeaders(oneprof_tool)

GetOpenCLTracingHeaders(oneprof_tool)

FindL0Library(oneprof_tool)
FindL0Headers(oneprof_tool)

CheckForMDLibrary(oneprof_tool)
CheckForMetricsLibrary()

# Loader

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOOL_NAME=oneprof_tool")
#Folloing is fixing an issue with libstdc++, depending on libpthread, but not linked against libpthread.
#Details: https://stackoverflow.com/questions/51209268/using-stdthread-in-a-library-loaded-with-dlopen-leads-to-a-sigsev
#Bug submission: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791 (fixed from gcc 10.5 and later)
add_link_options(-Wl,--no-as-needed -lpthread -Wl,--as-needed)
add_executable(oneprof "${PROJECT_SOURCE_DIR}/../../loader/loader.cc")
target_include_directories(oneprof
  PRIVATE "${PROJECT_SOURCE_DIR}/../../utils")
if(UNIX)
  target_link_libraries(oneprof
    dl)
endif()

# Installation

install(TARGETS oneprof oneprof_tool DESTINATION bin)