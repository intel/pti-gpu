include(macros)
RemoveNDebugFlag() # For SYCL Function Name
CheckExperimentalFilesystem()
CheckIfSyclIsAvailable()
GetGTest()

enable_testing()

include(GoogleTest)

if (NOT TARGET fmt::fmt)
  find_package(fmt REQUIRED)
endif()

if (NOT TARGET spdlog::spdlog)
  find_package(spdlog REQUIRED)
endif()

if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero REQUIRED)
endif()

if (NOT TARGET unified-runtime::loader)
  find_package(unified-runtime)
endif()

if (NOT TARGET Xpti::xpti)
  find_package(Xpti)
endif()

if (NOT TARGET unified-runtime::loader)
  message(STATUS "The Unified Runtime could not be found. Unified Runtime"
                 " specific tests will not be built.")

endif()

if (NOT TARGET Xpti::xpti)
  message(STATUS "Xpti could not be found. Xpti specific tests will not be"
                 " built.")
endif()

# Multi-config generators not supported yet, this is an incomplete bandage.
set(PTI_TEST_BIN_DIR "${PROJECT_BINARY_DIR}/bin")
get_property(PTI_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PTI_IS_MULTI_CONFIG)
  set(PTI_TEST_BIN_DIR "${PROJECT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
endif()

set(PTI_TEST_SPV_SRC ${PROJECT_SOURCE_DIR}/test/gemm.spv)
set(PTI_TEST_SPV_TEST ${PTI_TEST_BIN_DIR}/gemm.spv)

if(TARGET LevelZero::level-zero OR TARGET unified-runtime::loader)
  add_custom_command(
    OUTPUT ${PTI_TEST_SPV_TEST}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PTI_TEST_SPV_SRC} ${PTI_TEST_SPV_TEST}
    DEPENDS ${PTI_TEST_SPV_SRC})

  add_custom_target(
    copy_test_spv
    DEPENDS ${PTI_TEST_SPV_TEST}
  )
endif()

if (TARGET LevelZero::level-zero)
  add_executable(local_ze_collection_suite local_ze_collection_fixture.cc)
  target_include_directories(
    local_ze_collection_suite
    PUBLIC  "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")
   target_link_libraries(local_ze_collection_suite PUBLIC GTest::gmock
     GTest::gtest_main LevelZero::level-zero spdlog::spdlog Pti::pti_view)
   add_dependencies(local_ze_collection_suite copy_test_spv)
   gtest_discover_tests(
      local_ze_collection_suite
      DISCOVERY_TIMEOUT 60
      PROPERTIES LABELS "functional")
endif()

if(HAVE_SYCL)
  add_executable(dpcgemm_suite main_dpcgemm_fixture.cc)
  target_include_directories(
    dpcgemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(dpcgemm_suite PUBLIC -fsycl)
  target_link_options(dpcgemm_suite PUBLIC -fsycl)
  target_compile_definitions(dpcgemm_suite PUBLIC PTI_LEVEL_ZERO=1)
  target_link_libraries(dpcgemm_suite PUBLIC spdlog::spdlog Pti::pti_view
    GTest::gtest_main LevelZero::level-zero fmt::fmt)

  add_executable(classapi_suite main_classapi_fixture.cc)
  target_include_directories(
    classapi_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")


  target_compile_options( classapi_suite PUBLIC -fsycl)
  target_link_options(classapi_suite PUBLIC -fsycl)
  target_compile_definitions(classapi_suite PUBLIC PTI_LEVEL_ZERO=1)
  target_link_libraries(classapi_suite PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main LevelZero::level-zero)

  add_executable(vecsqadd_suite main_vecsqadd_fixture.cc)
  target_include_directories(
    vecsqadd_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    vecsqadd_suite
    PUBLIC -fsycl)

  target_link_options(vecsqadd_suite PUBLIC -fsycl)

  target_compile_definitions(vecsqadd_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(vecsqadd_suite PUBLIC spdlog::spdlog Pti::pti_view
    GTest::gtest_main)

  add_executable(vecsqadd_metrics_suite metrics_vecsqadd_fixture.cc)
  target_include_directories(
    vecsqadd_metrics_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils"

            "${PROJECT_SOURCE_DIR}/samples/samples_utilities")

  target_compile_options(
    vecsqadd_metrics_suite
    PUBLIC -fsycl)

  target_link_options(vecsqadd_metrics_suite PUBLIC -fsycl)

  target_compile_definitions(vecsqadd_metrics_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(vecsqadd_metrics_suite PUBLIC spdlog::spdlog
    GTest::gtest_main Pti::pti_view ${FS_LIB})




  add_executable(metrics_scope_gemm_suite metrics_scope_gemm.cc)
  target_include_directories(
    metrics_scope_gemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils"

            "${PROJECT_SOURCE_DIR}/samples/samples_utilities")

  target_compile_options(
    metrics_scope_gemm_suite
    PUBLIC -fsycl)

  target_link_options(metrics_scope_gemm_suite PUBLIC -fsycl)

  target_compile_definitions(metrics_scope_gemm_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(metrics_scope_gemm_suite PUBLIC spdlog::spdlog
    GTest::gtest_main Pti::pti_view ${FS_LIB})

  add_executable(metrics_scope_parallel metrics_scope_parallel.cc)
  target_include_directories(
    metrics_scope_parallel
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils"

            "${PROJECT_SOURCE_DIR}/samples/samples_utilities")

  target_compile_options(
    metrics_scope_parallel
    PUBLIC -fsycl)

  target_link_options(metrics_scope_parallel PUBLIC -fsycl)

  target_compile_definitions(metrics_scope_parallel PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(metrics_scope_parallel PUBLIC spdlog::spdlog
    GTest::gtest_main Pti::pti_view ${FS_LIB})


  add_executable(no_kernel_overlap no_kernel_overlap.cc)
  target_include_directories(
    no_kernel_overlap
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    no_kernel_overlap
    PUBLIC -fsycl)

  target_link_options(no_kernel_overlap PUBLIC -fsycl)

  target_compile_definitions(no_kernel_overlap PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(no_kernel_overlap PUBLIC spdlog::spdlog Pti::pti_view
	  GTest::gtest_main fmt::fmt)

  add_executable(startstop_suite main_startstop_fixture.cc)
  target_include_directories(
    startstop_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    startstop_suite
    PUBLIC -fsycl)

  target_link_options(startstop_suite PUBLIC -fsycl)

  target_compile_definitions(startstop_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(startstop_suite PUBLIC spdlog::spdlog Pti::pti_view
	  GTest::gtest_main)

  add_executable(mem_ops_test mem_ops_test.cc)

  target_include_directories(
    mem_ops_test
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(mem_ops_test PRIVATE -fsycl)

  target_link_options(mem_ops_test PRIVATE -fsycl)

  target_link_libraries(mem_ops_test PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main
    LevelZero::level-zero)

  add_executable(sycl_queue_id sycl_queue_id.cc)

  target_include_directories(
    sycl_queue_id
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(sycl_queue_id PRIVATE -fsycl)
  target_link_options(sycl_queue_id PRIVATE -fsycl)

  target_link_libraries(sycl_queue_id PUBLIC spdlog::spdlog Pti::pti_view
    GTest::gtest_main)

  add_executable(zegemm_suite main_zegemm_fixture.cc)

  target_include_directories(
    zegemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(zegemm_suite  PUBLIC -fsycl)
  target_link_options(zegemm_suite PUBLIC -fsycl)

  target_link_libraries(zegemm_suite PUBLIC spdlog::spdlog Pti::pti_view
                                            GTest::gtest_main
                                            GTest::gmock
                                            LevelZero::level-zero
                                            ${FS_LIB})

  add_executable(init_tests init_tests.cc)

  target_include_directories(
    init_tests
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(init_tests PUBLIC -fsycl)
  target_link_options(init_tests PUBLIC -fsycl)

  target_link_libraries(init_tests PUBLIC spdlog::spdlog Pti::pti_view
                                            GTest::gtest_main
                                            GTest::gmock
                                            LevelZero::level-zero
                                            ${FS_LIB})

  add_executable(multi_threaded_submission multi_threaded_submission.cc)

  target_include_directories(
    multi_threaded_submission
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(multi_threaded_submission PRIVATE -fsycl)
  target_link_options(multi_threaded_submission PRIVATE -fsycl)

  target_link_libraries(multi_threaded_submission PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main
                                            LevelZero::level-zero)

  add_executable(callback_api_test callback_api_test.cc)

  target_include_directories(
    callback_api_test
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero")

  target_compile_options(callback_api_test PUBLIC -fsycl)
  target_link_options(callback_api_test PUBLIC -fsycl)

  target_link_libraries(callback_api_test PUBLIC GTest::gtest_main spdlog::spdlog
                                          PRIVATE Pti::pti_view LevelZero::level-zero)


  add_executable(dpc_gemm_threaded_profiled perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_baseline perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_linkonly perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_prof_print perf_dpc_gemm_threaded.cc)

  target_include_directories(
    dpc_gemm_threaded_profiled
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/utils")

  target_include_directories(
    dpc_gemm_threaded_prof_print
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(dpc_gemm_threaded_profiled PRIVATE -fsycl)
  target_compile_options(dpc_gemm_threaded_baseline PRIVATE -fsycl)
  target_compile_options(dpc_gemm_threaded_linkonly PRIVATE -fsycl)
  target_compile_options(dpc_gemm_threaded_prof_print PRIVATE -fsycl)

  target_compile_definitions(dpc_gemm_threaded_baseline PRIVATE NO_PTI)
  target_compile_definitions(dpc_gemm_threaded_linkonly PRIVATE NO_PTI)
  target_compile_definitions(dpc_gemm_threaded_prof_print PRIVATE  RECORD_PARSE_AND_PRINT)

  target_link_options(dpc_gemm_threaded_profiled PRIVATE -fsycl)
  target_link_options(dpc_gemm_threaded_baseline PRIVATE -fsycl)
  target_link_options(dpc_gemm_threaded_linkonly PRIVATE -fsycl)
  target_link_options(dpc_gemm_threaded_prof_print PRIVATE -fsycl)

  target_link_libraries(dpc_gemm_threaded_profiled PUBLIC spdlog::spdlog
    Threads::Threads Pti::pti_view)

  target_link_libraries(dpc_gemm_threaded_linkonly PUBLIC spdlog::spdlog
    Threads::Threads Pti::pti_view)

  target_link_libraries(dpc_gemm_threaded_baseline PUBLIC Threads::Threads)

  target_link_libraries(dpc_gemm_threaded_prof_print PUBLIC spdlog::spdlog
    Threads::Threads Pti::pti_view)

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsUnset"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOff"
    PROPERTIES ENVIRONMENT PTI_VIEW_RUNTIME_API=0
    PROPERTIES ENVIRONMENT PTI_VIEW_DRIVER_API=0
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOn"
    PROPERTIES ENVIRONMENT PTI_VIEW_RUNTIME_API=1
    PROPERTIES ENVIRONMENT PTI_VIEW_DRIVER_API=1
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    mem_ops_test
    DISCOVERY_TIMEOUT 60
    TEST_LIST MEM_OPS_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "unit" )

  gtest_discover_tests(
    classapi_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    vecsqadd_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    vecsqadd_metrics_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       ENVIRONMENT ZET_ENABLE_METRICS=1
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics")

  gtest_discover_tests(
    metrics_scope_gemm_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       ENVIRONMENT ZET_ENABLE_METRICS=1
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics-scope")

  gtest_discover_tests(
    metrics_scope_parallel
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       ENVIRONMENT ZET_ENABLE_METRICS=1
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics-scope")

  gtest_discover_tests(
    sycl_queue_id
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    startstop_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    no_kernel_overlap
    DISCOVERY_TIMEOUT 60
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    zegemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_LIST ZEGEMM_SUITE_TEST_LIST
    PROPERTIES LABELS "unit")

  add_dependencies(zegemm_suite copy_test_spv)

  gtest_discover_tests(
    init_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    multi_threaded_submission
    DISCOVERY_TIMEOUT 60
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    callback_api_test
    PROPERTIES LABELS "functional")

  RequirePythonInterp()

  set(L0_OVERHEAD_THRESHOLD 17)
  set(LINK_OVERHEAD_THRESHOLD 3)
  if (WIN32)
    # Timer we use on Windows (as of implementation 02.14.25) has 100 ns resolution,
    # due to that some functions latency measured as zero, so accumulated overhead is less than on Linux
    set(L0_OVERHEAD_THRESHOLD 11)
    # For some reason, on Windows, the link overhead is bigger than on Linux
    # Need to investigate it - look into UR and XPTI
    set(LINK_OVERHEAD_THRESHOLD 5)
  endif()

  add_test(
    NAME perf-profiling-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 60 profiled
  )

  add_test(
    NAME perf-prof-gpu-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 32.5 prof-gpu
  )


  add_test(
    NAME perf-link-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" ${LINK_OVERHEAD_THRESHOLD} linkonly
  )

  add_test(
    NAME perf-overhead-view
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" ${L0_OVERHEAD_THRESHOLD} overhead
  )


  set_tests_properties(
    perf-profiling-overhead perf-link-overhead PROPERTIES LABELS "performance")

    set_tests_properties(
    perf-prof-gpu-overhead perf-link-overhead PROPERTIES LABELS "performance")

  set_tests_properties(
    perf-overhead-view PROPERTIES LABELS "performance;functional")

  # Add interoperability test
  add_test(
    NAME xpti_interop_test
    COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/xpti_interop_test.py "${PTI_TEST_BIN_DIR}"
  )
  set_tests_properties(
    xpti_interop_test PROPERTIES LABELS "interop;functional")

endif()

if (HAVE_SYCL AND TARGET unified-runtime::loader)

  add_executable(urgemm_suite main_urgemm_fixture.cc)

  target_include_directories(
    urgemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}"
           "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
           "${PROJECT_SOURCE_DIR}/src"
           "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(urgemm_suite  PUBLIC -fsycl)
  target_link_options(urgemm_suite PUBLIC -fsycl)
  target_link_libraries(urgemm_suite PUBLIC spdlog::spdlog
                                            Pti::pti_view
                                            GTest::gtest_main
                                            GTest::gmock
                                            LevelZero::level-zero
                                            unified-runtime::loader
                                            ${FS_LIB})
  gtest_discover_tests(
    urgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_LIST URGEMM_SUITE_TEST_LIST
    PROPERTIES LABELS "unit" "sycl" "unified-runtime")

  add_dependencies(urgemm_suite copy_test_spv)

  RequirePythonInterp()

  get_target_property(UR_HEADER_PATH unified-runtime::loader INTERFACE_INCLUDE_DIRECTORIES)

  if (UR_HEADER_PATH)
    add_test(
      NAME validate-sycl-ur-api-ids-test
      COMMAND ${Python_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/test/validate_sycl_api_ids.py --validate --pti-include-dir ${PROJECT_SOURCE_DIR}/include --ur-include-dir "${UR_HEADER_PATH}"
    )

    set_tests_properties(
      validate-sycl-ur-api-ids-test PROPERTIES LABELS "interop;functional")
  endif()

endif()

add_executable(view_record_test view_buffer_test.cc)

target_include_directories(
  view_record_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(view_record_test PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main)

add_executable(assert_exception_test assert_exception_test.cc
   "${PROJECT_SOURCE_DIR}/src/levelzero/ze_driver_init.cc")

target_include_directories(
  assert_exception_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(assert_exception_test PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main
                                                   LevelZero::level-zero
                                                   ${CMAKE_DL_LIBS}
                                                   ${FS_LIB})

add_executable(subscribers_unit_test subscribers_unit_test.cc)

target_include_directories(
  subscribers_unit_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(subscribers_unit_test PUBLIC GTest::gtest_main spdlog::spdlog
                                            PRIVATE Pti::pti_view LevelZero::headers)


add_executable(view_gpu_local_test view_gpu_local_test.cc)

target_include_directories(
  view_gpu_local_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(view_gpu_local_test PUBLIC spdlog::spdlog Pti::pti_view
  GTest::gtest_main)

add_executable(pti_assert_test pti_assert_test.cc)

target_include_directories(pti_assert_test PUBLIC
  "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(pti_assert_test PUBLIC GTest::gtest_main GTest::gmock)

gtest_discover_tests(
  pti_assert_test
  PROPERTIES LABELS "unit")

gtest_discover_tests(
  subscribers_unit_test
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
gtest_discover_tests(
  view_gpu_local_test
  DISCOVERY_TIMEOUT 60
  PROPERTIES LABELS "functional")

# TODO: Organize this better? Where to put
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
      "${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(
    NAME mt-awk-test-immediate
    COMMAND
      bash -c
      "${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT_IMMEDIATE} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT_IMMEDIATE}"
  )
  set_property(TEST mt-awk-test-immediate PROPERTY ENVIRONMENT SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1)
  set_property(TEST mt-awk-test PROPERTY ENVIRONMENT SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=0)

  add_test(NAME run-dlworkload COMMAND dlworkload)
  add_test(NAME run-dpc_gemm COMMAND dpc_gemm)
  add_test(NAME run-onemkl_gemm_exe COMMAND onemkl_gemm_exe)
  add_test(NAME run-vec_sqadd COMMAND vec_sqadd)
  add_test(NAME run-iso COMMAND iso3dfd 256 256 256 32 8 64 10 gpu)
  add_test(NAME run-metrics-iso COMMAND metrics_iso3dfd 256 256 256 32 8 64 10 gpu)
  add_test(NAME run-callback COMMAND callback)
  add_test(NAME run-metrics-scope COMMAND metrics_scope)
  add_test(NAME run-omp_vec_add COMMAND omp_vec_add)

  set_tests_properties(
    run-metrics-iso PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS
    "functional;samples;metrics;hw-metrics")

  set_tests_properties(
    mt-awk-test mt-awk-test-immediate run-dlworkload run-dpc_gemm run-onemkl_gemm_exe run-vec_sqadd
    run-iso run-callback PROPERTIES LABELS "functional;samples")

  set_tests_properties(
    run-metrics-scope PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS
    "functional;samples;metrics;hw-metrics-scope")

  # It is enabled only for functional test as there is an issue observed with Intel OpenMP with Address Sanitizer.
  set_tests_properties(run-omp_vec_add PROPERTIES LABELS "functional;samples")
endif()
