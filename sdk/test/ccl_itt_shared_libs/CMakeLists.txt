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
        LABELS "functional;itt;ccl-filter")

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

gtest_discover_tests(
    ccl_itt_dlopen_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
        LABELS "functional;itt;ccl-filter")
