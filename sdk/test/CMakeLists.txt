include(macros)
RemoveNDebugFlag() # For SYCL Function Name
CheckIfSyclIsAvailable()
GetGTest()

enable_testing()

include(GoogleTest)

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

if(HAVE_SYCL)
  add_executable(dpcgemm_suite main_dpcgemm_fixture.cc)
  target_include_directories(
    dpcgemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
           "${PROJECT_SOURCE_DIR}/src/opencl"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(dpcgemm_suite PUBLIC -fsycl -gline-tables-only)
  target_link_options(dpcgemm_suite PUBLIC -fsycl -gline-tables-only)
  target_compile_definitions(dpcgemm_suite PUBLIC PTI_LEVEL_ZERO=1)
  target_link_libraries(dpcgemm_suite PUBLIC Pti::pti_view GTest::gtest_main
    LevelZero::level-zero OpenCL::OpenCL)

  add_executable(vecsqadd_suite main_vecsqadd_fixture.cc)
  target_include_directories(
    vecsqadd_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
           "${PROJECT_SOURCE_DIR}/src/opencl"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    vecsqadd_suite
    PUBLIC -fsycl -gline-tables-only)

  target_link_options(vecsqadd_suite PUBLIC -fsycl -gline-tables-only)

  target_compile_definitions(vecsqadd_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(vecsqadd_suite PUBLIC Pti::pti_view
    GTest::gtest_main LevelZero::level-zero OpenCL::OpenCL)

  add_executable(no_kernel_overlap no_kernel_overlap.cc)
  target_include_directories(
    no_kernel_overlap
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    no_kernel_overlap
    PUBLIC -fsycl -gline-tables-only)

  target_link_options(no_kernel_overlap PUBLIC -fsycl -gline-tables-only)

  target_compile_definitions(no_kernel_overlap PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(no_kernel_overlap PUBLIC Pti::pti_view
    GTest::gtest_main LevelZero::level-zero OpenCL::OpenCL)

  add_executable(startstop_suite main_startstop_fixture.cc)
  target_include_directories(
    startstop_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(
    startstop_suite
    PUBLIC -fsycl -gline-tables-only)

  target_link_options(startstop_suite PUBLIC -fsycl -gline-tables-only)

  target_compile_definitions(startstop_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(startstop_suite PUBLIC Pti::pti_view
	  GTest::gtest_main LevelZero::headers)

  add_executable(mem_ops_test_ocl mem_ops_test_ocl.cc)
  add_executable(mem_ops_test_level_zero mem_ops_test_level_zero.cc)

  target_include_directories(
    mem_ops_test_ocl
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")
  target_include_directories(
    mem_ops_test_level_zero
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")


  target_compile_options(mem_ops_test_ocl PRIVATE -fsycl -gline-tables-only)
  target_compile_options(mem_ops_test_level_zero PRIVATE -fsycl -gline-tables-only)

  target_link_options(mem_ops_test_ocl PRIVATE -fsycl -gline-tables-only)
  target_link_options(mem_ops_test_level_zero PRIVATE -fsycl -gline-tables-only)

  target_link_libraries(mem_ops_test_ocl PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::level-zero OpenCL::OpenCL
                                            )
  target_link_libraries(mem_ops_test_level_zero PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::level-zero
                                            )

  add_executable(sycl_queue_id sycl_queue_id.cc)

  target_include_directories(
    sycl_queue_id
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(sycl_queue_id PRIVATE -fsycl -gline-tables-only)
  target_link_options(sycl_queue_id PRIVATE -fsycl -gline-tables-only)

  target_link_libraries(sycl_queue_id PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::headers)

  add_executable(zegemm_suite main_zegemm_fixture.cc)

  target_include_directories(
    zegemm_suite
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  add_custom_command(
    TARGET zegemm_suite
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${PROJECT_SOURCE_DIR}/test/gemm.spv ${PTI_TEST_BIN_DIR}/gemm.spv)
  add_custom_command(
    TARGET zegemm_suite
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${PROJECT_SOURCE_DIR}/test/gemm.cl ${PTI_TEST_BIN_DIR}/gemm.cl)

  target_compile_options(zegemm_suite  PUBLIC -fsycl -gline-tables-only)
  target_link_options(zegemm_suite PUBLIC -fsycl -gline-tables-only)

  target_link_libraries(zegemm_suite PUBLIC Pti::pti_view
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

  target_compile_options(multi_threaded_submission PRIVATE -fsycl -gline-tables-only)
  target_link_options(multi_threaded_submission PRIVATE -fsycl -gline-tables-only)

  target_link_libraries(multi_threaded_submission PUBLIC Pti::pti_view GTest::gtest_main
    LevelZero::level-zero OpenCL::OpenCL)

  add_executable(dpc_gemm_threaded_profiled perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_baseline perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_linkonly perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_overhead perf_dpc_gemm_threaded.cc)

  target_include_directories(
    dpc_gemm_threaded_profiled
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/utils")

 target_include_directories(
    dpc_gemm_threaded_overhead
    PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/utils")


  target_compile_options(dpc_gemm_threaded_profiled PRIVATE -fsycl -gline-tables-only)
  target_compile_options(dpc_gemm_threaded_baseline PRIVATE -fsycl -gline-tables-only)
  target_compile_options(dpc_gemm_threaded_linkonly PRIVATE -fsycl -gline-tables-only)
  target_compile_options(dpc_gemm_threaded_overhead PRIVATE -fsycl -gline-tables-only)

  target_compile_definitions(dpc_gemm_threaded_baseline PRIVATE NO_PTI)
  target_compile_definitions(dpc_gemm_threaded_linkonly PRIVATE NO_PTI)
  target_compile_definitions(dpc_gemm_threaded_overhead PRIVATE CAPTURE_OVERHEAD)

  target_link_options(dpc_gemm_threaded_profiled PRIVATE -fsycl -gline-tables-only)
  target_link_options(dpc_gemm_threaded_baseline PRIVATE -fsycl -gline-tables-only)
  target_link_options(dpc_gemm_threaded_linkonly PRIVATE -fsycl -gline-tables-only)
  target_link_options(dpc_gemm_threaded_overhead PRIVATE -fsycl -gline-tables-only)

  target_link_libraries(dpc_gemm_threaded_profiled PUBLIC Threads::Threads Pti::pti_view
    LevelZero::level-zero OpenCL::OpenCL)

  target_link_libraries(dpc_gemm_threaded_linkonly PUBLIC Threads::Threads Pti::pti_view
    LevelZero::level-zero OpenCL::OpenCL)

  target_link_libraries(dpc_gemm_threaded_baseline PUBLIC Threads::Threads)

  target_link_libraries(dpc_gemm_threaded_overhead PUBLIC Threads::Threads Pti::pti_view
                        LevelZero::level-zero)


  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsUnsetOcl"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOffOcl"
    PROPERTIES ENVIRONMENT PTI_TRACE_ALL_RUNTIME_OPS=0
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOnOcl"
    PROPERTIES ENVIRONMENT PTI_TRACE_ALL_RUNTIME_OPS=1
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsUnsetL0"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOffL0"
    PROPERTIES ENVIRONMENT PTI_TRACE_ALL_RUNTIME_OPS=0
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    dpcgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "RuntimeOpsSetOnL0"
    PROPERTIES ENVIRONMENT PTI_TRACE_ALL_RUNTIME_OPS=1
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    mem_ops_test_ocl
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    TEST_LIST MEM_OPS_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "unit" )

  gtest_discover_tests(
    mem_ops_test_level_zero
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    TEST_LIST MEM_OPS_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "unit" )

  gtest_discover_tests(
    vecsqadd_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    vecsqadd_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    sycl_queue_id
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    sycl_queue_id
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    startstop_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    startstop_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    no_kernel_overlap
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    no_kernel_overlap
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    zegemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_LIST ZEGEMM_SUITE_TEST_LIST
    PROPERTIES LABELS "unit")

  gtest_discover_tests(
    multi_threaded_submission
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "Ocl"
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "functional")

  gtest_discover_tests(
    multi_threaded_submission
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    TEST_LIST TIMESTAMPS_CORRECTNESS_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*"
    PROPERTIES LABELS "functional")

  RequirePythonInterp()
  add_test(
    NAME perf-profiling-overhead-ocl
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 66 profiled
  )

  add_test(
    NAME perf-link-overhead-ocl
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 8 linkonly
  )

  add_test(
    NAME perf-overhead-view
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 50 overhead
  )

  add_test(
    NAME perf-profiling-overhead-l0
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 66 profiled
  )

  add_test(
    NAME perf-link-overhead-l0
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 8 linkonly
  )

  set_tests_properties(
    perf-profiling-overhead-ocl perf-link-overhead-ocl
    perf-profiling-overhead-l0 perf-link-overhead-l0
    PROPERTIES LABELS "performance")

  set_tests_properties(
    perf-overhead-view PROPERTIES LABELS "performance;functional")

  set_tests_properties(
    perf-profiling-overhead-ocl
    perf-link-overhead-ocl
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu")

  set_tests_properties(
    perf-profiling-overhead-l0
    perf-link-overhead-l0
    perf-overhead-view
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu")


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

  add_custom_command(
    TARGET urgemm_suite
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/test/gemm.spv ${PTI_TEST_BIN_DIR}/gemm.spv)
  add_custom_command(
    TARGET urgemm_suite
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/test/gemm.cl ${PTI_TEST_BIN_DIR}/gemm.cl)

  target_compile_options(urgemm_suite  PUBLIC -fsycl -gline-tables-only)
  target_link_options(urgemm_suite PUBLIC -fsycl -gline-tables-only)
  target_link_libraries(urgemm_suite PUBLIC Pti::pti_view
                                            GTest::gtest_main
                                            GTest::gmock
                                            LevelZero::level-zero OpenCL::OpenCL
                                            unified-runtime::loader
                                            ${FS_LIB})
  gtest_discover_tests(
    urgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "OCL"
    TEST_LIST URGEMM_SUITE_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
    PROPERTIES LABELS "unit" "sycl" "unified-runtime")

  gtest_discover_tests(
    urgemm_suite
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "L0"
    TEST_LIST URGEMM_SUITE_TEST_LIST
    PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
    PROPERTIES LABELS "unit" "sycl" "unified-runtime")
endif()

add_executable(view_record_test view_buffer_test.cc)

target_include_directories(
  view_record_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/opencl"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(view_record_test PUBLIC Pti::pti_view GTest::gtest_main
                                              spdlog::spdlog
                                              LevelZero::level-zero
                                              OpenCL::OpenCL)

add_executable(assert_exception_test assert_exception_test.cc
                            "${PROJECT_SOURCE_DIR}/src/utils/trace_guard.cc")

get_target_property(TRACING_HEADER_DIR OpenCL::TracingHeaders  INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(
  assert_exception_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${TRACING_HEADER_DIR}"
         "${PROJECT_SOURCE_DIR}/src/opencl"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(assert_exception_test PUBLIC Pti::pti_view GTest::gtest_main
                                                   spdlog::spdlog
                                                   LevelZero::level-zero
                                                   OpenCL::OpenCL
                                                   ${CMAKE_DL_LIBS}
                                                   ${FS_LIB})

add_executable(view_gpu_local_test view_gpu_local_test.cc)

target_include_directories(
  view_gpu_local_test
  PUBLIC "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(view_gpu_local_test PUBLIC Pti::pti_view GTest::gtest_main
                                            LevelZero::headers)

add_executable(pti_assert_test pti_assert_test.cc)

target_include_directories(pti_assert_test PUBLIC
  "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(pti_assert_test PUBLIC GTest::gtest_main)

gtest_discover_tests(
  zegemm_suite
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "OCL"
  TEST_LIST ZEGEMM_SUITE_TEST_LIST
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  zegemm_suite
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "L0"
  TEST_LIST ZEGEMM_SUITE_TEST_LIST
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  view_record_test
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "OCL"
  TEST_LIST VIEW_RECORD_TEST_LIST
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
  PROPERTIES LABELS "unit")
gtest_discover_tests(
  view_record_test
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "L0"
  TEST_LIST VIEW_RECORD_TEST_LIST
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
  PROPERTIES LABELS "unit")
#gtest_discover_tests(
#  assert_exception_test
#  DISCOVERY_TIMEOUT 60
#  TEST_LIST ASSERT_EXCEPTION_TEST_LIST
#  PROPERTIES LABELS "unit")

gtest_discover_tests(
  view_gpu_local_test
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "OCL"
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=opencl:gpu"
  PROPERTIES LABELS "functional")
gtest_discover_tests(
  view_gpu_local_test
  DISCOVERY_TIMEOUT 60
  TEST_PREFIX "L0"
  PROPERTIES ENVIRONMENT "ONEAPI_DEVICE_SELECTOR=level_zero:gpu"
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
    NAME mt-awk-test-ocl
    COMMAND
      bash -c
      "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(
    NAME mt-awk-test-l0
    COMMAND
      bash -c
      "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(
    NAME mt-awk-test-all-BE
    COMMAND
      bash -c
      "${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT}"
  )
  add_test(
    NAME mt-awk-test-immediate-ocl
    COMMAND
      bash -c
      "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT_IMMEDIATE} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT_IMMEDIATE}"
  )
  add_test(
    NAME mt-awk-test-immediate-l0
    COMMAND
      bash -c
      "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT_IMMEDIATE} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT_IMMEDIATE}"
  )
  add_test(
    NAME mt-awk-test-immediate-all-BE
    COMMAND
      bash -c
      "${PTI_TEST_BIN_DIR}/dpc_gemm_threaded -t ${TEST_THREADS} > ${MT_AWK_TEST_OUT_IMMEDIATE} &&
          awk -f ${CMAKE_CURRENT_SOURCE_DIR}/multi_thread_correlation.awk ${MT_AWK_TEST_OUT_IMMEDIATE}"
  )
  set_property(TEST mt-awk-test-immediate-ocl PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1,ONEAPI_DEVICE_SELECTOR=opencl:gpu")
  set_property(TEST mt-awk-test-immediate-l0 PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1,ONEAPI_DEVICE_SELECTOR=level_zero:gpu")
  set_property(TEST mt-awk-test-immediate-all-BE PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1")
  set_property(TEST mt-awk-test-ocl PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=0,ONEAPI_DEVICE_SELECTOR=opencl:gpu")
  set_property(TEST mt-awk-test-l0 PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=0,ONEAPI_DEVICE_SELECTOR=level_zero:gpu")
  set_property(TEST mt-awk-test-all-BE PROPERTY ENVIRONMENT "SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=0")

  #add_test(NAME run-dlworkload-ocl COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/dlworkload")
  add_test(NAME run-dlworkload-l0 COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/dlworkload")
  add_test(NAME run-dpc_gemm-ocl COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm")
  add_test(NAME run-dpc_gemm-l0 COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/dpc_gemm")
  add_test(NAME run-onemkl_gemm_exe-ocl COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/onemkl_gemm_exe")
  add_test(NAME run-onemkl_gemm_exe-l0 COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/onemkl_gemm_exe")
  add_test(NAME run-vec_sqadd-ocl COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/vec_sqadd")
  add_test(NAME run-vec_sqadd-l0 COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/vec_sqadd")
  add_test(NAME run-iso-ocl COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=opencl:gpu ${PTI_TEST_BIN_DIR}/iso3dfd 256 256 256 32 8 64 10 gpu")
  add_test(NAME run-iso-l0 COMMAND bash -c "ONEAPI_DEVICE_SELECTOR=level_zero:gpu ${PTI_TEST_BIN_DIR}/iso3dfd 256 256 256 32 8 64 10 gpu")
  # run-metrics-iso is sporadically failing on windows due to a bug in L0. Disabling on Windows until
  # issue is resolved
  if(UNIX)
    add_test(NAME run-metrics-iso COMMAND metrics_iso3dfd 256 256 256 32 8 64 10 gpu)

    set_tests_properties(
      run-metrics-iso PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS "functional;samples;hw-metrics")
  endif()

  set_tests_properties(
    mt-awk-test-ocl mt-awk-test-immediate-ocl mt-awk-test-l0 mt-awk-test-immediate-l0 run-dlworkload-l0 run-dpc_gemm-ocl run-dpc_gemm-l0 run-onemkl_gemm_exe-ocl run-onemkl_gemm_exe-l0 run-vec_sqadd-ocl run-vec_sqadd-l0
    run-iso-ocl run-iso-l0 PROPERTIES LABELS "functional;samples")

endif()
