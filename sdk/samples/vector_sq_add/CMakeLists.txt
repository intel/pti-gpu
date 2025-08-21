cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_KERNEL_V CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requres a sycl compatible compiler")
  return()
endif()

add_executable(vec_sqadd vector_sq_add.cc)

target_link_options(vec_sqadd PUBLIC -fsycl)
target_compile_options(vec_sqadd PUBLIC -fsycl)
target_include_directories(vec_sqadd
                           PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities")

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(vec_sqadd PUBLIC Pti::pti_view)

