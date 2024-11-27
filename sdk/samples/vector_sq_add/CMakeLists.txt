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
  message(WARNING "${PROJECT_NAME} requres a sycl compatible compiler")
  return()
endif()

add_executable(vec_sqadd vector_sq_add.cc)

target_link_options(vec_sqadd PUBLIC -fsycl -gline-tables-only)
target_compile_options(vec_sqadd PUBLIC -fsycl -gline-tables-only)


target_include_directories(vec_sqadd
                           PRIVATE "${PROJECT_SOURCE_DIR}/../../src/utils"
                            "${PROJECT_SOURCE_DIR}/../samples_utilities")

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()


if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero REQUIRED)
endif()

target_link_libraries(vec_sqadd PUBLIC Pti::pti_view LevelZero::level-zero)


