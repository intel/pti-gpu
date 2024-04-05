getgtest()

enable_testing()

include(GoogleTest)

if(HAVE_SYCL)
  add_executable(dpcgemm_suite main_dpcgemm_fixture.cc)
  target_include_directories(
    dpcgemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
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

  target_link_libraries(vecsqadd_suite PUBLIC Pti::pti_view
                                              GTest::gtest_main)

  add_executable(no_kernel_overlap no_kernel_overlap.cc)
  target_include_directories(
    no_kernel_overlap
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")
#           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"

  target_compile_options(
    no_kernel_overlap
    PUBLIC -fsycl -gline-tables-only
    PRIVATE ${PTI_COMPILE_FLAGS_EXPR})

  target_link_options(no_kernel_overlap PUBLIC -fsycl -gline-tables-only PRIVATE
                      ${PTI_LINK_FLAGS_EXPR})

  target_compile_definitions(no_kernel_overlap PUBLIC PTI_LEVEL_ZERO=1
                                                   ${PTI_DEFINE_FLAGS_EXPR})

  target_link_libraries(no_kernel_overlap PUBLIC Pti::pti_view
                                              GTest::gtest_main)

  add_executable(mem_ops_test mem_ops_test.cc)

  target_include_directories(
    mem_ops_test
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/syclpi" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(mem_ops_test PRIVATE -fsycl -gline-tables-only
                                                ${PTI_COMPILE_FLAGS_EXPR})
  target_link_options(mem_ops_test PRIVATE -fsycl -gline-tables-only
                    ${PTI_LINK_FLAGS_EXPR})
  target_compile_definitions(mem_ops_test PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

  target_link_libraries(mem_ops_test PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::level-zero)

  add_executable(sycl_queue_id sycl_queue_id.cc)

  target_include_directories(
    sycl_queue_id
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/syclpi" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(sycl_queue_id PRIVATE -fsycl -gline-tables-only
                                                ${PTI_COMPILE_FLAGS_EXPR})
  target_link_options(sycl_queue_id PRIVATE -fsycl -gline-tables-only
                    ${PTI_LINK_FLAGS_EXPR})
  target_compile_definitions(sycl_queue_id PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

  target_link_libraries(sycl_queue_id PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::level-zero)

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    mem_ops_test
    DISCOVERY_TIMEOUT 60
    TEST_LIST MEM_OPS_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "unit" )

  gtest_discover_tests(
    vecsqadd_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    sycl_queue_id
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")


  gtest_discover_tests(
    no_kernel_overlap
    DISCOVERY_TIMEOUT 60
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "functional")
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

if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero REQUIRED)
endif()

target_compile_options(zegemm_suite PRIVATE ${PTI_COMPILE_FLAGS_EXPR})
target_link_options(zegemm_suite PRIVATE ${PTI_LINK_FLAGS_EXPR})
target_compile_definitions(zegemm_suite PRIVATE ${PTI_DEFINE_FLAGS_EXPR})

target_link_libraries(zegemm_suite PUBLIC Pti::pti_view GTest::gtest_main
                                          GTest::gmock LevelZero::level-zero)

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
                                              spdlog::spdlog_header_only
                                              LevelZero::level-zero)

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
                                                   spdlog::spdlog_header_only
                                                   LevelZero::level-zero)

gtest_discover_tests(
  zegemm_suite
  DISCOVERY_TIMEOUT 60
  TEST_LIST ZEGEMM_SUITE_TEST_LIST
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  view_record_test
  DISCOVERY_TIMEOUT 60
  TEST_LIST VIEW_RECORD_TEST_LIST
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  assert_exception_test
  DISCOVERY_TIMEOUT 60
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
  set(MT_AWK_TEST_OUT_IMMEDIATE "${CMAKE_BINARY_DIR}/mt-awk-test-immediate.out")
  add_test(
    NAME mt-awk-test
    COMMAND
      bash -c
      "${CMAKE_BINARY_DIR}/samples/dpc_gemm_threaded/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(
    NAME mt-awk-test-immediate
    COMMAND
      bash -c
      "${CMAKE_BINARY_DIR}/samples/dpc_gemm_threaded/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT_IMMEDIATE} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT_IMMEDIATE}"
  )
  set_property(TEST mt-awk-test-immediate PROPERTY ENVIRONMENT SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1)
  set_property(TEST mt-awk-test PROPERTY ENVIRONMENT SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=0)

  add_test(NAME run-dlworkload COMMAND dlworkload)
  add_test(NAME run-dpc_gemm COMMAND dpc_gemm)
  add_test(NAME run-onemkl_gemm_exe COMMAND onemkl_gemm_exe)
  add_test(NAME run-vec_sqadd COMMAND vec_sqadd)
  add_test(NAME run-iso COMMAND iso3dfd.exe 256 256 256 32 8 64 10 gpu)

  set_tests_properties(
    mt-awk-test mt-awk-test-immediate run-dlworkload run-dpc_gemm run-onemkl_gemm_exe run-vec_sqadd
    run-iso PROPERTIES LABELS "functional;samples")
endif()
