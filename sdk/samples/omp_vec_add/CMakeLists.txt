cmake_minimum_required(VERSION 3.14)

project(PTI_Samples_OMP_VEC_ADD C)

# compiler needs to be checked, if not icx then return early with warning message
if (NOT CMAKE_C_COMPILER_ID STREQUAL "IntelLLVM")
  message(WARNING "${PROJECT_NAME} requires a Intel OMP enabled compiler")
  return()
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXCompilerFlag)

add_executable(omp_vec_add main.c)

target_include_directories(omp_vec_add PRIVATE "${PROJECT_SOURCE_DIR}/../samples_utilities")

set(OPENMP_FLAGS
  $<$<PLATFORM_ID:Linux>:-fiopenmp -fopenmp-targets=spir64>
  $<$<PLATFORM_ID:Windows>:-Qiopenmp -Qopenmp-targets=spir64>
)

target_compile_options(omp_vec_add PUBLIC ${OPENMP_FLAGS})
target_link_options(omp_vec_add PUBLIC ${OPENMP_FLAGS})

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_link_libraries(omp_vec_add PUBLIC Pti::pti_view)
