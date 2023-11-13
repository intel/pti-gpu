getgtest()

enable_testing()

include(GoogleTest)

if(HAVE_SYCL)
  add_executable(dpcgemm_suite main_dpcgemm_fixture.cc)
  target_include_directories(
    dpcgemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    dpcgemm_suite
    PUBLIC -fsycl -gline-tables-only
    PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
  target_link_options(dpcgemm_suite PUBLIC -fsycl -gline-tables-only PRIVATE
                      ${PTI_LINK_FLAGS_EXPR})
  target_compile_definitions(dpcgemm_suite PUBLIC PTI_LEVEL_ZERO=1
                                                  ${PTI_DEFINE_FLAGS_EXPR})

  target_link_libraries(dpcgemm_suite PUBLIC Pti::pti_view GTest::gtest_main)

  add_executable(vecsqadd_suite main_vecsqadd_fixture.cc)
  target_include_directories(
    vecsqadd_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    vecsqadd_suite
    PUBLIC -fsycl -gline-tables-only
    PRIVATE ${PTI_COMPILE_FLAGS_EXPR})

  target_link_options(vecsqadd_suite PUBLIC -fsycl -gline-tables-only PRIVATE
                      ${PTI_LINK_FLAGS_EXPR})

  target_compile_definitions(vecsqadd_suite PUBLIC PTI_LEVEL_ZERO=1
                                                   ${PTI_DEFINE_FLAGS_EXPR})

  target_link_libraries(vecsqadd_suite PUBLIC Pti::pti_view xptifw
                                              GTest::gtest_main)

  gtest_discover_tests(dpcgemm_suite PROPERTIES LABELS "unit")
  gtest_discover_tests(vecsqadd_suite PROPERTIES LABELS "functional")
endif()

add_executable(zegemm_suite main_zegemm_fixture.cc)

target_include_directories(
  zegemm_suite
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

add_custom_command(
  TARGET zegemm_suite
  PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${PROJECT_SOURCE_DIR}/test/gemm.spv ${CMAKE_BINARY_DIR}/test/gemm.spv)
add_custom_command(
  TARGET zegemm_suite
  PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${PROJECT_SOURCE_DIR}/test/gemm.cl ${CMAKE_BINARY_DIR}/test/gemm.cl)

findl0library(zegemm_suite)
findl0headers(zegemm_suite)

target_compile_options(zegemm_suite PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(zegemm_suite PRIVATE ${PTI_LINK_FLAGS_EXPR})
target_compile_definitions(zegemm_suite PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

target_link_libraries(zegemm_suite PUBLIC Pti::pti_view GTest::gtest_main
                                          GTest::gmock)

add_executable(view_record_test view_buffer_test.cc)

target_include_directories(
  view_record_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/syclpi" "${PROJECT_SOURCE_DIR}/src/utils")

target_compile_options(view_record_test PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(view_record_test PRIVATE ${PTI_LINK_FLAGS_EXPR})
target_compile_definitions(view_record_test PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

target_link_libraries(view_record_test PUBLIC Pti::pti_view GTest::gtest_main
                                              spdlog::spdlog_header_only)

add_executable(assert_exception_test assert_exception_test.cc)

target_include_directories(
  assert_exception_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/syclpi" "${PROJECT_SOURCE_DIR}/src/utils")

target_compile_options(assert_exception_test PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(assert_exception_test PRIVATE ${PTI_LINK_FLAGS_EXPR})
target_compile_definitions(assert_exception_test PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

target_link_libraries(assert_exception_test PUBLIC Pti::pti_view GTest::gtest_main
                                              spdlog::spdlog_header_only)

gtest_discover_tests(
  zegemm_suite
  TEST_LIST ZEGEMM_SUITE_TEST_LIST
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  view_record_test
  TEST_LIST VIEW_RECORD_TEST_LIST
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  assert_exception_test
  TEST_LIST ASSERT_EXCEPTION_TEST_LIST
  PROPERTIES LABELS "unit")

# TODO(matthew.schilling@intel.com): Organize this better? Where to put
# functional tests?
# Runs all samples
if(PTI_BUILD_SAMPLES AND HAVE_SYCL)
  include(ProcessorCount)
  ProcessorCount(NPROC)
  if(NOT NPROC LESS_EQUAL 1)
    math(EXPR TEST_THREADS "${NPROC} / 2" OUTPUT_FORMAT DECIMAL)
  else()
    set(TEST_THREADS 2)
  endif()
  set(MT_AWK_TEST_OUT "${CMAKE_BINARY_DIR}/mt-awk-test.out")
  add_test(
    NAME mt-awk-test
    COMMAND
      bash -c
      "${CMAKE_BINARY_DIR}/samples/dpc_gemm_threaded/dpc_gemm_threaded ${TEST_THREADS} > ${MT_AWK_TEST_OUT} && awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(NAME run-dlworkload COMMAND dlworkload)
  add_test(NAME run-dpc_gemm COMMAND dpc_gemm)
  add_test(NAME run-onemkl_gemm_exe COMMAND onemkl_gemm_exe)
  add_test(NAME run-vec_sqadd COMMAND vec_sqadd)
  add_test(NAME run-iso COMMAND iso3dfd.exe 256 256 256 32 8 64 10 gpu)

  set_tests_properties(
    mt-awk-test run-dlworkload run-dpc_gemm run-onemkl_gemm_exe run-vec_sqadd
    run-iso PROPERTIES LABELS "functional;samples")
endif()
