#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

find_package(Threads REQUIRED)

if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero)
endif()

if (TARGET LevelZero::level-zero)
  add_executable(ze_graph_suite ze_graph_fixture.cc)

  target_link_libraries(ze_graph_suite PUBLIC Pti::pti_view GTest::gtest_main
    spdlog::spdlog fmt::fmt Threads::Threads LevelZero::level-zero LevelZero::level-zero-ext)

  target_include_directories(
    ze_graph_suite
    PUBLIC  "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_BINARY_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/test"
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/utils"
            "${PROJECT_SOURCE_DIR}/src/levelzero"
  )

  add_spirv(ze_graph_suite ze_graph_test_kernels)

  gtest_discover_tests(
    ze_graph_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       LABELS "functional"
       LABELS "graph"
       LABELS "level-zero"
       ENVIRONMENT "PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000")
endif()

if(NOT HAVE_SYCL)
  message(STATUS "Skipping SYCL graph tests: SYCL not available")
  return()
endif()

add_executable(sycl_graph_suite sycl_graph_fixture.cc)

target_compile_options(sycl_graph_suite PUBLIC -fsycl)
target_link_options(sycl_graph_suite PUBLIC -fsycl)

target_link_libraries(sycl_graph_suite PUBLIC Pti::pti_view GTest::gtest_main
  fmt::fmt Threads::Threads)

target_include_directories(
  sycl_graph_suite
  PUBLIC  "${PROJECT_SOURCE_DIR}/include"
          "${PROJECT_BINARY_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/test"
)

gtest_discover_tests(
  sycl_graph_suite
  DISCOVERY_TIMEOUT 60
  PROPERTIES
     LABELS "functional"
     LABELS "graph"
     LABELS "sycl"
     ENVIRONMENT "PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000")
