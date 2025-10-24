cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_METRICS_SCOPE CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# TODO: find_package(IntelSYCL)?
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if (NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler")
  return()
endif()

add_executable(metrics_scope main_metrics_scope.cc client.cc)

target_link_options(metrics_scope PUBLIC -fsycl)
target_compile_options(metrics_scope PUBLIC -fsycl)

target_include_directories(metrics_scope PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities")

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(metrics_scope PUBLIC Pti::pti_view LevelZero::level-zero)
