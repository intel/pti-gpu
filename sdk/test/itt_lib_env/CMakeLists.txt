#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

add_executable(itt_lib_env_test itt_lib_env_test.cc)

target_include_directories(itt_lib_env_test PUBLIC
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/utils"
  "${PROJECT_BINARY_DIR}")

target_link_libraries(itt_lib_env_test PUBLIC GTest::gtest_main
                                              spdlog::spdlog
                                              fmt::fmt
                                              ittapi::headers
                                              ${CMAKE_DL_LIBS}
                                              ${FS_LIB})

add_dependencies(itt_lib_env_test pti_view)

gtest_discover_tests(
  itt_lib_env_test
  DISCOVERY_TIMEOUT 60
  PROPERTIES
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
    LABELS "unit"
    LABELS "itt")

add_test(NAME itt_lib_env_standalone COMMAND itt_lib_env_test)
set_tests_properties(itt_lib_env_standalone PROPERTIES
  ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
  LABELS "unit;itt")
