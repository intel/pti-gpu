#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Add library subdirectories
add_subdirectory(ccladd)
add_subdirectory(foomul)

# Linked-library variant of the oneCCL domain-filtering tests.
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

# Runtime-loaded variant of the same domain-filtering tests.
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

# Verify that libpti_view.so self-registers before ccladd.so's first ITT call
# when INTEL_LIBITTNOTIFY64 is empty. ccladd.so models oneCCL calls originating
# from a separate shared library, and FiltersCclBasic confirms their collection.
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
