#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Add library subdirectories
add_subdirectory(ccladd)
add_subdirectory(foomul)

# CCL ITT libraries test
add_executable(ccl_itt_libs_test ccl_itt_libs_test.cc)

target_include_directories(
  ccl_itt_libs_test
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_BINARY_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils"
          "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
)
target_compile_options(ccl_itt_libs_test PUBLIC -fsycl)
target_link_options(ccl_itt_libs_test PUBLIC -fsycl)
target_link_libraries(ccl_itt_libs_test PUBLIC
  GTest::gmock
  GTest::gtest_main
  spdlog::spdlog
  fmt::fmt
  Pti::pti_view
  ittapi::ittnotify
  LevelZero::level-zero
  ccladd
  foomul
  ${FS_LIB})

  gtest_discover_tests(
    ccl_itt_libs_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
        LABELS "functional"
        LABELS "itt"
        LABELS "ccl-filter")

# CCL ITT dlopen test — same domain-filter tests but libraries loaded at runtime
add_executable(ccl_itt_dlopen_test ccl_itt_dlopen_test.cc)
target_include_directories(
  ccl_itt_dlopen_test
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_BINARY_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils"
          "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
)
target_compile_options(ccl_itt_dlopen_test PUBLIC -fsycl)
target_link_options(ccl_itt_dlopen_test PUBLIC -fsycl)
target_compile_definitions(ccl_itt_dlopen_test PRIVATE
  CCLADD_LIB_PATH="$<TARGET_FILE:ccladd>"
  FOOMUL_LIB_PATH="$<TARGET_FILE:foomul>")
target_link_libraries(ccl_itt_dlopen_test PUBLIC
  GTest::gmock
  GTest::gtest_main
  spdlog::spdlog
  fmt::fmt
  Pti::pti_view
  LevelZero::level-zero
  ${CMAKE_DL_LIBS}
  ${FS_LIB})
add_dependencies(ccl_itt_dlopen_test ccladd foomul)

#
# Validates that GlobalIttInitializer in libpti_view.so sets
# INTEL_LIBITTNOTIFY64 at load time, so ccladd.so's ITT calls are collected
# without any external env var setup. FiltersCclBasic is sufficient: it
# exercises the full path from self-registration through record collection.
# INTEL_LIBITTNOTIFY64 is intentionally empty here.
#
# ccladd.so is a minimal stand-in for oneCCL: a shared library that wraps
# arithmetic operations with ITT task annotations under the "oneCCL::API"
# domain. It models the critical topology - ITT calls coming from a separate
# shared library - without requiring a real oneCCL installation. The ittnotify
# latch fires when ccladd.so makes its first ITT call, which is exactly the
# race condition GlobalIttInitializer is designed to win.
#
add_test(
  NAME ccl_itt_collects_without_env_var
  COMMAND ccl_itt_libs_test --gtest_filter=CclDomainFilterTest.FiltersCclBasic)
set_tests_properties(ccl_itt_collects_without_env_var PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "INTEL_LIBITTNOTIFY64="
  FAIL_REGULAR_EXPRESSION "did not match any test"
  LABELS "functional;itt;ccl-filter")

gtest_discover_tests(
    ccl_itt_dlopen_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
        LABELS "functional"
        LABELS "itt"
        LABELS "ccl-filter")
