# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# List of Environment Variables and their usage in these tests.
# PTI_CONV_CLOCK_SYNC_TIME_NS:              Time for synchronization between MONOTONIC_RAW (internal) and REALTIME clocks. Needed for timestamp comparison between graph replays.
# NEOReadDebugKeys:                         Enables driver debug environment variables.
# ExperimentalFlatCommandListApiRecording:  Enables dynamic tracing of SYCL graph replays (without tracing graph creation).

find_package(Threads REQUIRED)

if(NOT TARGET LevelZero::level-zero)
  find_package(LevelZero)
endif()

if(NOT TARGET LevelZero::level-zero)
  message(STATUS "Level Zero needed to build graph tests.")
  return()
endif()

add_executable(ze_graph_suite ze_graph_fixture.cc)

target_link_libraries(
  ze_graph_suite
  PUBLIC Pti::pti_view
         GTest::gtest_main
         spdlog::spdlog
         fmt::fmt
         Threads::Threads
         LevelZero::level-zero
         LevelZero::level-zero-ext)

target_include_directories(
  ze_graph_suite
  PUBLIC "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/test" "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils"
          "${PROJECT_SOURCE_DIR}/src/levelzero")

add_spirv(ze_graph_suite ze_graph_test_kernels)

gtest_discover_tests(
  ze_graph_suite DISCOVERY_TIMEOUT 60
  PROPERTIES LABELS
             "functional"
             LABELS
             "graph"
             LABELS
             "level-zero"
             ENVIRONMENT
             "PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000")

add_executable(
  ze_command_visitor_tests
  ${PROJECT_SOURCE_DIR}/src/levelzero/ze_driver_init.cc
  ${PROJECT_SOURCE_DIR}/src/levelzero/ze_command_visitor.cc
  ze_command_visitor_tests.cc)

target_include_directories(
  ze_command_visitor_tests
  PUBLIC "${PROJECT_SOURCE_DIR}/src/levelzero"
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/utils"
          "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}"
          "${PROJECT_BINARY_DIR}/include")

target_link_libraries(
  ze_command_visitor_tests
  PUBLIC GTest::gmock GTest::gtest_main LevelZero::level-zero
         LevelZero::level-zero-ext spdlog::spdlog fmt::fmt)

gtest_discover_tests(
  ze_command_visitor_tests DISCOVERY_TIMEOUT 60
  PROPERTIES LABELS
             "functional"
             LABELS
             "internal"
             LABELS
             "level-zero"
             ENVIRONMENT
             "NEOReadDebugKeys=1"
             ENVIRONMENT
             "ExperimentalFlatCommandListApiRecording=1")

if(NOT HAVE_SYCL)
  message(STATUS "Skipping SYCL graph tests: SYCL not available")
  return()
endif()

add_executable(sycl_graph_suite sycl_graph_fixture.cc)

target_compile_options(sycl_graph_suite PUBLIC -fsycl)
target_link_options(sycl_graph_suite PUBLIC -fsycl)

target_link_libraries(
  sycl_graph_suite PUBLIC Pti::pti_view GTest::gtest_main fmt::fmt spdlog::spdlog
                          Threads::Threads LevelZero::level-zero)

target_include_directories(
  sycl_graph_suite
  PUBLIC "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/test" "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils")

gtest_discover_tests(
  sycl_graph_suite DISCOVERY_TIMEOUT 60
  PROPERTIES LABELS
             "functional"
             LABELS
             "graph"
             LABELS
             "sycl"
             ENVIRONMENT
             "PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000"
             ENVIRONMENT
             "NEOReadDebugKeys=1"
             ENVIRONMENT
             "ExperimentalFlatCommandListApiRecording=1")
