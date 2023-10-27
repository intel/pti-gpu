set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Modules/")
include(macros)

setrequiredcmakeversion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_Samples_KERNEL_V CXX)
setcompilerflags()
setbuildtype()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(FATAL_ERROR "${PROJECT_NAME} requres a sycl compatible compiler")
endif()

add_compile_options(-fsycl -gline-tables-only)
add_link_options(-fsycl -gline-tables-only)

add_executable(vec_sqadd vector_sq_add.cpp)
target_include_directories(vec_sqadd PRIVATE ../samples_utilities)

# If not building as part of PTI
get_directory_property(subproject PARENT_DIRECTORY)
if(NOT subproject)
  if(NOT PTI_BUILD_DIR)
    set(PTI_BUILD_DIR "${CMAKE_SOURCE_DIR}/../../build")
  endif()

  # TODO(matthew.schilling@intel.com): find_package first?
  if(NOT EXISTS ${PTI_BUILD_DIR})
    message(
      FATAL_ERROR
        "Unable to find PTI build directory. Please pass valid build directory with flag -DPTI_BUILD_DIR=/path/to/dir"
    )
  endif()

  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PTI_BUILD_DIR}/cmake")
  include(PtiTargets OPTIONAL RESULT_VARIABLE FOUND_PTI_BUILD_TARGETS)
  if(NOT FOUND_PTI_BUILD_TARGETS)
    message(FATAL_ERROR "Unable to find PTI library targets")
  endif()
endif()

target_link_libraries(vec_sqadd PUBLIC xptifw Pti::pti_view)

target_include_directories(vec_sqadd
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils")
if(CMAKE_INCLUDE_PATH)
  target_include_directories(vec_sqadd PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
