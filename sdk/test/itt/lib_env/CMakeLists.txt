#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

add_library(itt_lib_env_external SHARED itt_lib_env_fixture.cc)

# Compile the decision helper without the interface's startup registration.
add_executable(itt_lib_env_test itt_lib_env_test.cc
  "${PROJECT_SOURCE_DIR}/src/itt/itt_lib_env.cc")

target_compile_definitions(itt_lib_env_test PRIVATE
  PTI_TEST_LOADER_EXTERNAL_LIBRARY="$<TARGET_FILE:itt_lib_env_external>")
# Let the bare fixture name resolve through the loader even before it is loaded.
set_property(TARGET itt_lib_env_test APPEND PROPERTY BUILD_RPATH
  "$<TARGET_FILE_DIR:itt_lib_env_external>")

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

add_dependencies(itt_lib_env_test pti_view itt_lib_env_external)

# Register each case separately for precise CTest failure reporting.
gtest_discover_tests(
  itt_lib_env_test
  DISCOVERY_TIMEOUT 60
  PROPERTIES
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
    LABELS "unit"
    LABELS "itt")

# Also run the complete suite in one process to verify that no helper state
# leaks between cases.
add_test(NAME itt_lib_env_standalone COMMAND itt_lib_env_test)
set_tests_properties(itt_lib_env_standalone PROPERTIES
  ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
  LABELS "unit;itt")
