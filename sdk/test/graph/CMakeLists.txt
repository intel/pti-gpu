#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# For now we only have DPC++ SYCL graph tests. Be a little more targeted when
# (or if) we have other kinds of workloads.
if(NOT HAVE_SYCL)
  message(STATUS "Skipping SYCL graph tests: SYCL not available")
  return()
endif()

find_package(Threads REQUIRED)

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
     ENVIRONMENT "PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000")
