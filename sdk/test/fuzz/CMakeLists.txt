add_executable(pti_view_fuzz test_pti_view.cc)

target_link_libraries(pti_view_fuzz PUBLIC Pti::pti_view)

target_compile_options(pti_view_fuzz PUBLIC -fsanitize=fuzzer,address)
target_link_options(pti_view_fuzz PUBLIC -fsanitize=fuzzer,address)

target_include_directories(pti_view_fuzz PRIVATE
  "${PROJECT_SOURCE_DIR}/src/utils")

# Use CTest has a test runner
add_test(NAME fuzz-pti-view-lib COMMAND pti_view_fuzz)

# https://github.com/intel/compute-runtime/issues/376#issuecomment-786029828
set_tests_properties(
  fuzz-pti-view-lib PROPERTIES LABELS "fuzz" ENVIRONMENT
                                  "NEOReadDebugKeys=1;DisableDeepBind=1")
