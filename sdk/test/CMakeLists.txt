#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(macros)
include(add_spirv)

RemoveNDebugFlag() # For SYCL Function Name
CheckExperimentalFilesystem()
CheckIfSyclIsAvailable()
GetGTest()

enable_testing()

message(STATUS "Configuring tests")

include(GoogleTest)

if (NOT TARGET fmt::fmt)
  message(FATAL_ERROR "fmt::fmt target not found. GetSpdlog() should have been called in the main CMakeLists.txt")
endif()

if (NOT TARGET spdlog::spdlog)
  find_package(spdlog REQUIRED)
endif()

if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero REQUIRED)
endif()

# Detects whether the system Level Zero loader exports zetDeviceEnableMetricsExp
# and zetDeviceDisableMetricsExp. On Linux uses check_library_exists against
# libze_loader.so; on Windows uses dumpbin /EXPORTS against ze_loader.dll in
# System32. Sets HAVE_ZET_DEVICE_METRIC_API=TRUE if both symbols are present.
# Must be called after LevelZero::level-zero target is available.
DetectSystemLevelZeroMetricApi()

# Metric tests enable metrics at runtime through ptiMetricsEnable(). When the system
# loader does not export the on-demand APIs, that call cannot work and the tests need
# ZET_ENABLE_METRICS=1 in the environment instead -- the L0 loader latches this variable
# at zeInit(), so it must come from ctest rather than from inside the test process.
# Kept as a list so that no empty ENVIRONMENT entry is passed when the APIs are present.
set(PTI_METRIC_TEST_PROPS "")
if(NOT HAVE_ZET_DEVICE_METRIC_API)
  list(APPEND PTI_METRIC_TEST_PROPS ENVIRONMENT "ZET_ENABLE_METRICS=1")
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

GetLevelZeroExtensions()

# Multi-config generators not supported yet, this is an incomplete bandage.
set(PTI_TEST_BIN_DIR "${PROJECT_BINARY_DIR}/bin")
get_property(PTI_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PTI_IS_MULTI_CONFIG)
  set(PTI_TEST_BIN_DIR "${PROJECT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
endif()

# Dependency pre-load tests. libld_dependency_preload_test.cc documents the layout
# these libraries make up: the test executable stands in for the PTI interface
# library and the "core" stubs in preload_stage/pti for the PTI Core library.
#
# Linux only: what this exercises is ELF specific - resolution by soname, $ORIGIN
# based RUNPATH, and the loader error text MissingSonameFromLoaderError parses.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PTI_PRELOAD_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/preload_stage")

  # Builds synthetic library pti_preload_<id> from libld_preload_stub.c.
  # OUTPUT_DIR controls its location; NEEDS <id> and ALSO_NEEDS <id> add DT_NEEDED dependencies.
  # Only ORIGIN_RUNPATH gives it RUNPATH $ORIGIN; other stubs have no RPATH.
  function(AddPreloadStub id)
    cmake_parse_arguments(STUB "UNDEFINED_SYMBOL;ORIGIN_RUNPATH" "OUTPUT_DIR;NEEDS;ALSO_NEEDS" "" ${ARGN})

    set(stub_target "pti_preload_${id}")
    add_library(${stub_target} SHARED "${CMAKE_CURRENT_SOURCE_DIR}/libld_preload_stub.c")
    target_compile_definitions(${stub_target} PRIVATE "PRELOAD_STUB_NAME=${id}")

    set(stub_install_rpath "")
    if(STUB_ORIGIN_RUNPATH)
      set(stub_install_rpath "$ORIGIN")
    endif()
    set_target_properties(
      ${stub_target}
      PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${STUB_OUTPUT_DIR}"
                 # Overrides CMAKE_BUILD_RPATH. Most stubs must leave their
                 # dependencies unresolved so the pre-load path can recover them.
                 BUILD_WITH_INSTALL_RPATH ON
                 INSTALL_RPATH "${stub_install_rpath}")

    # Same reason the product libraries do this: keep the Intel compiler's runtime
    # libraries off DT_NEEDED, so a stub cannot fail over a dependency the test
    # never set up.
    target_compile_options(${stub_target} PRIVATE ${PTI_INTEL_LLVM_EXCLUDE_LIBS})
    target_link_options(${stub_target} PRIVATE ${PTI_INTEL_LLVM_EXCLUDE_LIBS})

    if(STUB_NEEDS)
      target_compile_definitions(${stub_target} PRIVATE "PRELOAD_STUB_NEEDS=${STUB_NEEDS}")
      target_link_libraries(${stub_target} PRIVATE "pti_preload_${STUB_NEEDS}")
    endif()

    if(STUB_ALSO_NEEDS)
      target_compile_definitions(${stub_target} PRIVATE "PRELOAD_STUB_ALSO_NEEDS=${STUB_ALSO_NEEDS}")
      target_link_libraries(${stub_target} PRIVATE "pti_preload_${STUB_ALSO_NEEDS}")
    endif()

    if(STUB_UNDEFINED_SYMBOL)
      target_compile_definitions(${stub_target} PRIVATE PRELOAD_STUB_UNDEFINED_SYMBOL)
    endif()

    add_dependencies(libld_dependency_preload_test ${stub_target})
  endfunction()

  add_executable(libld_dependency_preload_test libld_dependency_preload_test.cc)

  target_include_directories(
    libld_dependency_preload_test
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/utils"
            "${PROJECT_SOURCE_DIR}/include"
            ${PROJECT_BINARY_DIR}
            "${PROJECT_BINARY_DIR}/include")

  target_link_libraries(libld_dependency_preload_test PRIVATE GTest::gtest_main spdlog::spdlog
    ${CMAKE_DL_LIBS} ${FS_LIB})

  # The trace log is what makes a failure here readable. Compiling the macros in is
  # only half of it; the test raises the runtime level, see LogLevelFromEnvironment.
  target_compile_definitions(libld_dependency_preload_test PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

  set_target_properties(
    libld_dependency_preload_test
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PTI_PRELOAD_STAGE_DIR}"
               # Stands in for the interface library's RUNPATH, which is what makes
               # the loader's search reach the stage directory and nothing above it.
               BUILD_WITH_INSTALL_RPATH ON
               INSTALL_RPATH "$ORIGIN")

  # Where the Core library is deployed, also the 1st fallback search directory.
  set(PTI_PRELOAD_CORE_DIR "${PTI_PRELOAD_STAGE_DIR}/${PTI_MODULE_SUBDIR}")

  # Next to the test executable, so the loader's own search reaches it through
  # RUNPATH $ORIGIN. This is the wheel's case.
  AddPreloadStub(dep_bysoname OUTPUT_DIR "${PTI_PRELOAD_STAGE_DIR}")
  # Next to the core libraries instead: in a fallback search directory but outside
  # RUNPATH, so only naming the file outright reaches it.
  AddPreloadStub(dep_byfullpath OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}")
  # A dependency of a dependency: reachable, but not before chain_tail is.
  AddPreloadStub(dep_chain_tail OUTPUT_DIR "${PTI_PRELOAD_STAGE_DIR}")
  AddPreloadStub(dep_chain_head OUTPUT_DIR "${PTI_PRELOAD_STAGE_DIR}" NEEDS dep_chain_tail)
  # Reachable neither way.
  AddPreloadStub(dep_unreachable OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/preload_unreachable")
  # Recovery pre-loads tail, then head. Core's other direct dependency remains unreachable.
  AddPreloadStub(dep_cleanup_tail OUTPUT_DIR "${PTI_PRELOAD_STAGE_DIR}")
  AddPreloadStub(dep_cleanup_head OUTPUT_DIR "${PTI_PRELOAD_STAGE_DIR}" NEEDS dep_cleanup_tail)
  AddPreloadStub(dep_cleanup_missing OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/preload_unreachable")

  # Stand-ins for the PTI Core library, one per dependency shape above.
  AddPreloadStub(core_bysoname OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}" NEEDS dep_bysoname)
  AddPreloadStub(core_byfullpath OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}" NEEDS dep_byfullpath)
  AddPreloadStub(core_chain OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}" NEEDS dep_chain_head)
  AddPreloadStub(core_unreachable OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}" NEEDS dep_unreachable)
  AddPreloadStub(core_cleanup_failure OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}"
                 NEEDS dep_cleanup_head ALSO_NEEDS dep_cleanup_missing)
  AddPreloadStub(core_undefined OUTPUT_DIR "${PTI_PRELOAD_CORE_DIR}" UNDEFINED_SYMBOL)

  # Ordinary ELF resolution case: the synthetic Core and its dependency share a
  # directory, and Core alone carries RUNPATH $ORIGIN.
  set(PTI_ORIGIN_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/origin_stage")
  AddPreloadStub(dep_origin OUTPUT_DIR "${PTI_ORIGIN_STAGE_DIR}")
  AddPreloadStub(core_origin OUTPUT_DIR "${PTI_ORIGIN_STAGE_DIR}" NEEDS dep_origin ORIGIN_RUNPATH)

  add_executable(libld_origin_test libld_origin_test.cc)
  add_dependencies(libld_origin_test pti_preload_core_origin)
  target_compile_definitions(
    libld_origin_test
    PRIVATE PTI_ORIGIN_CORE_LIBRARY_PATH="$<TARGET_FILE:pti_preload_core_origin>")
  target_link_libraries(libld_origin_test PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS})

  gtest_discover_tests(
    libld_dependency_preload_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       LABELS "unit"
       LABELS "libload")
endif()

if (TARGET LevelZero::level-zero)
  add_executable(ze_event_manager_tests ze_event_manager_tests.cc)
  target_include_directories(
    ze_event_manager_tests
    PUBLIC "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/utils"
            "${PROJECT_SOURCE_DIR}/include"
            ${PROJECT_BINARY_DIR}
            "${PROJECT_BINARY_DIR}/include"
  )

  target_link_libraries(ze_event_manager_tests PUBLIC GTest::gmock
     GTest::gtest_main LevelZero::level-zero spdlog::spdlog fmt::fmt)

   gtest_discover_tests(
      ze_event_manager_tests
      DISCOVERY_TIMEOUT 60
      PROPERTIES LABELS "unit")

  add_executable(ze_kernel_name_cache_test ze_kernel_name_cache_test.cc)
  target_include_directories(
    ze_kernel_name_cache_test
    PUBLIC "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/utils"
            "${PROJECT_SOURCE_DIR}/include"
            ${PROJECT_BINARY_DIR}
            "${PROJECT_BINARY_DIR}/include")

  target_link_libraries(ze_kernel_name_cache_test PUBLIC
    GTest::gtest_main LevelZero::level-zero spdlog::spdlog fmt::fmt)

  gtest_discover_tests(
    ze_kernel_name_cache_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "unit")

  add_executable(pti_string_pool_test pti_string_pool_test.cc)
  target_include_directories(
    pti_string_pool_test
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_link_libraries(pti_string_pool_test PUBLIC GTest::gtest_main)

  gtest_discover_tests(
    pti_string_pool_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "unit")

  add_executable(local_ze_collection_suite local_ze_collection_fixture.cc)

  target_include_directories(
    local_ze_collection_suite
    PUBLIC  "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_link_libraries(local_ze_collection_suite PUBLIC GTest::gmock
     GTest::gtest_main LevelZero::level-zero LevelZero::level-zero-ext spdlog::spdlog Pti::pti_view)

  add_spirv(local_ze_collection_suite gemm)

  gtest_discover_tests(
      local_ze_collection_suite
      DISCOVERY_TIMEOUT 60
      PROPERTIES LABELS "functional")
endif()

if(HAVE_SYCL)
  add_executable(dpcgemm_suite main_dpcgemm_fixture.cc)
  target_include_directories(
    dpcgemm_suite
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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

  # Metric enable/disable tests. These exercise the on-demand ptiMetricsEnable path and
  # skip themselves when ZET_ENABLE_METRICS=1 is set, so they deliberately do not get
  # ${PTI_METRIC_TEST_PROPS}.
  add_executable(metric_enable_disable_test metric_enable_disable_test.cc)
  target_include_directories(
    metric_enable_disable_test
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_definitions(metric_enable_disable_test PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(metric_enable_disable_test PUBLIC spdlog::spdlog
    GTest::gtest_main Pti::pti_view LevelZero::level-zero)

  gtest_discover_tests(
    metric_enable_disable_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "unit")




  add_executable(metrics_scope_gemm_suite metrics_scope_gemm.cc)
  target_include_directories(
    metrics_scope_gemm_suite
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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

  add_executable(metrics_scope_buffer_handler_suite metrics_scope_buffer_handler.cc)
  target_include_directories(
    metrics_scope_buffer_handler_suite
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_link_libraries(metrics_scope_buffer_handler_suite PUBLIC spdlog::spdlog
    GTest::gtest_main Pti::pti_view ${FS_LIB})

  add_executable(metrics_scope_parallel metrics_scope_parallel.cc)
  target_include_directories(
    metrics_scope_parallel
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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


  if(PTI_BUILD_SAMPLES AND TARGET metrics_scope)
    add_test(NAME metrics_scope_multiprocess
      COMMAND ${Python_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_metrics_scope_multiprocess.py
        --sample-binary $<TARGET_FILE:metrics_scope>)
    set_tests_properties(metrics_scope_multiprocess PROPERTIES
      ENVIRONMENT ZET_ENABLE_METRICS=1
      # 77 is the GNU Autotools / CTest convention for "test skipped".
      SKIP_RETURN_CODE 77
      TIMEOUT 360
      LABELS "functional;metrics;hw-metrics-scope;multiprocess")
  endif()

  # Add the multi-threaded test
  add_executable(
    multi_thread_metrics_suite
    metrics_multi_thread.cc)
  target_include_directories(
    multi_thread_metrics_suite
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils"
            "${PROJECT_SOURCE_DIR}/samples/samples_utilities")

  target_compile_options(
    multi_thread_metrics_suite
    PUBLIC -fsycl)

  target_link_options(multi_thread_metrics_suite PUBLIC -fsycl)

  target_compile_definitions(multi_thread_metrics_suite PUBLIC PTI_LEVEL_ZERO=1)

  target_link_libraries(multi_thread_metrics_suite PUBLIC spdlog::spdlog
    Threads::Threads GTest::gtest_main Pti::pti_view ${FS_LIB})

  add_executable(no_kernel_overlap no_kernel_overlap.cc)
  target_include_directories(
    no_kernel_overlap
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}/include"
           "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(mem_ops_test PRIVATE -fsycl)

  target_link_options(mem_ops_test PRIVATE -fsycl)

  target_link_libraries(mem_ops_test PUBLIC spdlog::spdlog Pti::pti_view
    GTest::gtest_main GTest::gmock
    LevelZero::level-zero)

  add_executable(sycl_queue_id sycl_queue_id.cc)

  target_include_directories(
    sycl_queue_id
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
  add_spirv(zegemm_suite gemm)

  add_executable(init_tests init_tests.cc)

  target_include_directories(
    init_tests
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
           "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero")

  target_compile_options(callback_api_test PUBLIC -fsycl)
  target_link_options(callback_api_test PUBLIC -fsycl)

  target_link_libraries(callback_api_test PUBLIC GTest::gtest_main spdlog::spdlog
                                          PRIVATE Pti::pti_view LevelZero::level-zero)

  add_executable(callback_kernel_lifecycle_test callback_kernel_lifecycle_test.cc)

  target_include_directories(
    callback_kernel_lifecycle_test
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
      PRIVATE "${PROJECT_SOURCE_DIR}/src/utils")

  target_compile_options(callback_kernel_lifecycle_test PUBLIC -fsycl)
  target_link_options(callback_kernel_lifecycle_test PUBLIC -fsycl)

  target_link_libraries(callback_kernel_lifecycle_test
    PUBLIC GTest::gtest_main spdlog::spdlog
    PRIVATE Pti::pti_view LevelZero::level-zero)

  add_spirv(callback_kernel_lifecycle_test gemm)

  add_executable(dpc_gemm_threaded_profiled perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_baseline perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_linkonly perf_dpc_gemm_threaded.cc)
  add_executable(dpc_gemm_threaded_prof_print perf_dpc_gemm_threaded.cc)

  target_include_directories(
    dpc_gemm_threaded_profiled
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
         "${PROJECT_SOURCE_DIR}/src/utils")

  target_include_directories(
    dpc_gemm_threaded_prof_print
    PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
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
       ${PTI_METRIC_TEST_PROPS}
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics")

  # Add the multi-threaded metrics vecsqadd test
  gtest_discover_tests(
    multi_thread_metrics_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        ${PTI_METRIC_TEST_PROPS}
        TIMEOUT 20
        LABELS "functional"
        LABELS "metrics"
        LABELS "hw-metrics-multi-thread")

  gtest_discover_tests(
    metrics_scope_gemm_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       ${PTI_METRIC_TEST_PROPS}
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics-scope")

  gtest_discover_tests(
    metrics_scope_buffer_handler_suite
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       LABELS "functional"
       LABELS "metrics"
       LABELS "hw-metrics-scope"
       LABELS "buffer-handler")

  gtest_discover_tests(
    metrics_scope_parallel
    DISCOVERY_TIMEOUT 60
    PROPERTIES
       ${PTI_METRIC_TEST_PROPS}
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

  gtest_discover_tests(
    callback_kernel_lifecycle_test
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

  # Approximate total runtime budget (seconds) for each performance test,
  # passed to perf_test.py. The script divides it across all subprocess
  # invocations to derive a per-process timeout, with a 10s minimum floor,
  # so the actual worst-case runtime may exceed this value. It is intentionally
  # approximate: the goal is to bound runaway hangs, not enforce a hard deadline.
  # No ctest TIMEOUT is set here: perf_test.py spawns subprocesses and is the
  # only level that can kill them cleanly on timeout. A ctest-level kill would
  # terminate the Python script but leave the child process orphaned and still
  # holding GPU resources, which is exactly the hang this mechanism prevents.
  set(PTI_PERF_TEST_APROXIMATE_TIMEOUT 60)

  set(PTI_PERF_TESTS
    perf-profiling-overhead
    perf-prof-gpu-overhead
    perf-link-overhead
    perf-overhead-view)

  add_test(
    NAME perf-profiling-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 60 profiled
      ${PTI_PERF_TEST_APROXIMATE_TIMEOUT}
  )

  add_test(
    NAME perf-prof-gpu-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" 32.5 prof-gpu
      ${PTI_PERF_TEST_APROXIMATE_TIMEOUT}
  )

  add_test(
    NAME perf-link-overhead
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" ${LINK_OVERHEAD_THRESHOLD} linkonly
      ${PTI_PERF_TEST_APROXIMATE_TIMEOUT}
  )

  add_test(
    NAME perf-overhead-view
    COMMAND
      ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/perf_test.py "${PTI_TEST_BIN_DIR}" ${L0_OVERHEAD_THRESHOLD} overhead
      ${PTI_PERF_TEST_APROXIMATE_TIMEOUT}
  )

  set_tests_properties(
    ${PTI_PERF_TESTS} PROPERTIES
    LABELS "performance")

  set_tests_properties(
    perf-overhead-view PROPERTIES
    SKIP_RETURN_CODE 77
    LABELS "performance;functional")

  # Add interoperability test
  add_test(
    NAME xpti_interop_test
    COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/xpti_interop_test.py "${PTI_TEST_BIN_DIR}"
  )
  set_tests_properties(
    xpti_interop_test PROPERTIES LABELS "interop;functional")

endif()

add_subdirectory(graph)

if (HAVE_SYCL AND TARGET unified-runtime::loader)

  if (PTI_BUILD_URGEMM)
    add_executable(urgemm_suite main_urgemm_fixture.cc)

    target_include_directories(
      urgemm_suite
      PUBLIC "${PROJECT_BINARY_DIR}"
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

    add_spirv(urgemm_suite gemm)
  endif()

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
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(view_record_test PUBLIC spdlog::spdlog Pti::pti_view GTest::gtest_main)

get_itt()

add_executable(assert_exception_test assert_exception_test.cc)

target_include_directories(
  assert_exception_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/include" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(assert_exception_test PUBLIC spdlog::spdlog
  GTest::gtest_main LevelZero::headers)

add_executable(subscribers_unit_test subscribers_unit_test.cc)

target_include_directories(
  subscribers_unit_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(subscribers_unit_test PUBLIC GTest::gtest_main spdlog::spdlog
                                            PRIVATE Pti::pti_view LevelZero::headers)

add_executable(ze_events_pools_observer_test ze_events_pools_observer_test.cc)

target_include_directories(ze_events_pools_observer_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/include" "${PROJECT_SOURCE_DIR}/include"
         "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/levelzero"
         "${PROJECT_SOURCE_DIR}/src/sycl" "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(ze_events_pools_observer_test PUBLIC spdlog::spdlog
                                                    GTest::gtest_main
                                                   LevelZero::level-zero
                                                   ${CMAKE_DL_LIBS})

add_executable(view_gpu_local_test view_gpu_local_test.cc)

target_include_directories(
  view_gpu_local_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(view_gpu_local_test PUBLIC spdlog::spdlog Pti::pti_view
  GTest::gtest_main)

add_executable(pc_sampling_basic_test pc_sampling/pc_sampling_basic_test.cc)

target_include_directories(
  pc_sampling_basic_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils"
          "${CMAKE_CURRENT_SOURCE_DIR}/pc_sampling")

target_link_libraries(pc_sampling_basic_test PUBLIC spdlog::spdlog Pti::pti_view
  GTest::gtest_main LevelZero::level-zero LevelZero::level-zero-ext ${FS_LIB})

add_executable(pc_sampling_internal_test pc_sampling/pc_sampling_internal_test.cc)

target_include_directories(
  pc_sampling_internal_test
  PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/utils"
          "${CMAKE_CURRENT_SOURCE_DIR}/pc_sampling")

target_link_libraries(pc_sampling_internal_test PUBLIC spdlog::spdlog Pti::pti_view
GTest::gtest_main LevelZero::level-zero LevelZero::level-zero-ext ${FS_LIB})

if(HAVE_SYCL)
  add_executable(pc_sampling_functional_test pc_sampling/pc_sampling_functional_test.cc)

  target_compile_options(pc_sampling_functional_test PRIVATE -fsycl)
  target_link_options(pc_sampling_functional_test PRIVATE -fsycl)

  target_link_libraries(pc_sampling_functional_test PUBLIC spdlog::spdlog Pti::pti_view
    GTest::gtest_main LevelZero::level-zero LevelZero::level-zero-ext ${FS_LIB})
endif()

add_executable(pti_assert_test pti_assert_test.cc)

target_include_directories(pti_assert_test PUBLIC
  "${PROJECT_SOURCE_DIR}/src/utils")

target_link_libraries(pti_assert_test PUBLIC GTest::gtest_main GTest::gmock)

gtest_discover_tests(
  pti_assert_test
  PROPERTIES LABELS "unit")

add_executable(pti_memory_route_test pti_memory_route_test.cc)

target_include_directories(pti_memory_route_test PUBLIC
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_BINARY_DIR}/include"
  "${PROJECT_SOURCE_DIR}/src/utils"
  "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(pti_memory_route_test PUBLIC GTest::gtest_main GTest::gmock)

gtest_discover_tests(
  pti_memory_route_test
  PROPERTIES LABELS "unit")

gtest_discover_tests(
  subscribers_unit_test
  PROPERTIES LABELS "unit")

gtest_discover_tests(
  ze_events_pools_observer_test
  DISCOVERY_TIMEOUT 60
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

gtest_discover_tests(
  pc_sampling_basic_test
  DISCOVERY_TIMEOUT 60
  PROPERTIES
    ${PTI_METRIC_TEST_PROPS}
    LABELS "unit")

gtest_discover_tests(
  pc_sampling_internal_test
  DISCOVERY_TIMEOUT 60
  PROPERTIES
    ${PTI_METRIC_TEST_PROPS}
    LABELS "unit"
    LABELS "metrics"
    LABELS "hw-metrics")

if(HAVE_SYCL)
  gtest_discover_tests(
    pc_sampling_functional_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
      ${PTI_METRIC_TEST_PROPS}
      LABELS "functional"
      LABELS "metrics"
      LABELS "hw-metrics")
endif()

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
  if(DEFINED PTI_SANITIZER AND PTI_SANITIZER AND TEST_THREADS GREATER 8)
    set(TEST_THREADS 8)
  endif()
  set(MT_AWK_TEST_OUT "${PROJECT_BINARY_DIR}/mt-awk-test.out")
  set(MT_AWK_TEST_OUT_IMMEDIATE "${PROJECT_BINARY_DIR}/mt-awk-test-immediate.out")
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

  if(TEST_THREADS GREATER 16)
    set_property(TEST mt-awk-test APPEND PROPERTY ENVIRONMENT "UR_L0_USM_RESIDENT=0")
  endif()

  if (TARGET dlworkload)
    add_test(NAME run-dlworkload COMMAND dlworkload)
    set_tests_properties(run-dlworkload PROPERTIES LABELS "functional;samples")

    # Profile Time Difference test for dlworkload
    add_test(
      NAME prof-time-diff-dlworkload
      COMMAND
        ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/prof_time_diff/prof_time_diff.py
        $<TARGET_FILE:dlworkload>
        # allow here absolute time difference of 4 us (4000 ns) which should be acceptable
        # for long running kernels, and for ~ usec duration short ones - this is measurement error
        -da 4000
        # skip first 50 kernels as warm-up
        -s 50
        # set delta for the second run to 50 us, much smaller than default 300 us
        -e2 PTI_DEVICE_SYNC_DELTA=50000
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )

    # TODO(PTI-422): Evaluate flaky test
    set_tests_properties(
      prof-time-diff-dlworkload PROPERTIES LABELS "functional;prof-time-diff;quarantine")
  endif()

  add_test(NAME run-dpc_gemm COMMAND dpc_gemm)
  add_test(NAME run-onemkl_gemm_exe COMMAND onemkl_gemm_exe)
  add_test(NAME run-vec_sqadd COMMAND vec_sqadd)
  add_test(NAME run-iso COMMAND iso3dfd 256 256 256 32 8 64 10 gpu)
  add_test(NAME run-metrics-iso COMMAND metrics_iso3dfd 256 256 256 32 8 64 10 gpu)
  add_test(NAME run-callback COMMAND callback)
  add_test(NAME run-metrics-scope COMMAND metrics_scope)
  add_test(NAME run-omp_vec_add COMMAND omp_vec_add)
  add_test(NAME run-metrics-perf COMMAND metrics_perf)

  # TODO(PTI-354): Re-enable test when it stops being flaky or hanging.
  set_tests_properties(
    run-metrics-iso PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS
    "functional;samples;metrics;hw-metrics")

  set_tests_properties(
    mt-awk-test mt-awk-test-immediate run-dpc_gemm run-onemkl_gemm_exe run-vec_sqadd
    run-iso run-callback PROPERTIES LABELS "functional;samples")

  set_tests_properties(
    run-metrics-scope PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS
    "functional;samples;metrics;hw-metrics-scope")

  set_tests_properties(
    run-metrics-perf PROPERTIES ENVIRONMENT "ZET_ENABLE_METRICS=1" LABELS
    "functional;samples;metrics;hw-metrics-multi-thread")

  # It is enabled only for functional test as there is an issue observed with Intel OpenMP with Address Sanitizer.
  set_tests_properties(run-omp_vec_add PROPERTIES LABELS "functional;samples")
endif()

# CCL ITT tests
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HAVE_SYCL)
  get_itt()

  add_executable(itt_test_suite itt_test_suite.cc)
  target_include_directories(
    itt_test_suite
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
     "${PROJECT_BINARY_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
      "${PROJECT_SOURCE_DIR}/src/utils"
      "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
  )
  target_compile_options(itt_test_suite PUBLIC -fsycl)
  target_link_options(itt_test_suite PUBLIC -fsycl)
  target_link_libraries(itt_test_suite PUBLIC GTest::gmock
               GTest::gtest_main
               spdlog::spdlog
               fmt::fmt
               Pti::pti_view
               ittapi::ittnotify
               LevelZero::level-zero
               ${FS_LIB})

  # Minimal external ITT collector used by
  # IttTest.ExternalTrivialCollectorDisablesPtiTracing.
  add_library(itt_trivial_collector SHARED itt_trivial_collector.cc)
  target_link_libraries(itt_trivial_collector PRIVATE
    ittapi::headers
    spdlog::spdlog)
  target_compile_definitions(itt_trivial_collector PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
  add_dependencies(itt_test_suite itt_trivial_collector)

  #
  # Bug warning in CMake: it does not accept LABELS separated by ';'.
  #
  gtest_discover_tests(
     itt_test_suite
     DISCOVERY_TIMEOUT 60
     PROPERTIES
        ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
        LABELS "unit"
        LABELS "itt")

  #
  # Validates the self-registration path (GlobalIttInitializer sets
  # INTEL_LIBITTNOTIFY64 at load time when it is not already set). Running
  # only Task_Handlecreate_Begin_End is sufficient: if self-registration works,
  # the ITT collector is loaded and records flow; the test body confirms that.
  # INTEL_LIBITTNOTIFY64 is intentionally empty here.
  #
  add_test(
    NAME itt_self_registration
    COMMAND itt_test_suite --gtest_filter=IttTest.Task_Handlecreate_Begin_End)
  set_tests_properties(itt_self_registration PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64="
    FAIL_REGULAR_EXPRESSION "did not match any test"
    LABELS "unit;itt")

  # PTI is preloaded by a relative path. The child changes directory before
  # loading PTI again, so the inherited collector path must be absolute.
  string(CONCAT PTI_ITT_CHILD_COMMAND
    "test -n \"$INTEL_LIBITTNOTIFY64\" && "
    "unset LD_PRELOAD && cd /tmp && "
    "exec \"$1\" --gtest_filter=IttTest.Task_Handlecreate_Begin_End")
  add_test(
    NAME itt_child_inherits_self_registration
    COMMAND ${CMAKE_COMMAND} -E chdir
            "$<TARGET_FILE_DIR:Pti::pti_view>"
            ${CMAKE_COMMAND} -E env
              "INTEL_LIBITTNOTIFY64="
              "LD_PRELOAD=./$<TARGET_FILE_NAME:Pti::pti_view>"
              /bin/sh -c "${PTI_ITT_CHILD_COMMAND}"
              sh $<TARGET_FILE:itt_test_suite>)
  set_tests_properties(itt_child_inherits_self_registration PROPERTIES
    TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "did not match any test"
    LABELS "unit;itt")

  # A pre-set path to PTI remains valid and enables PTI collection.
  #
  add_test(
    NAME itt_preconfigured_pti_collector_enables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.PreconfiguredPtiCollectorEnablesPtiTracing)
  set_tests_properties(itt_preconfigured_pti_collector_enables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>;PTI_TEST_EXPECT_PRECONFIGURED_PTI_COLLECTOR=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # itt_trivial_collector must receive the ITT calls while PTI remains disabled.
  add_test(
    NAME itt_external_trivial_collector_disables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.ExternalTrivialCollectorDisablesPtiTracing)
  set_tests_properties(itt_external_trivial_collector_disables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:itt_trivial_collector>;PTI_TEST_EXPECT_NO_ITT_COLLECTION=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # CCL ITT shared libraries test
  add_subdirectory(ccl_itt_shared_libs)

  # Real-library preload integration test. The stage models a wheel layout: Core is in
  # pti/, while the interface and the complete non-system XPTI runtime closure are one
  # directory above it. No ambient loader path is permitted.
  if(TARGET Xpti::xpti)
    set(PTI_LIB_LOAD_PRELOAD_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib_load_preload")
    set(PTI_LIB_LOAD_PRELOAD_CORE_DIR
        "${PTI_LIB_LOAD_PRELOAD_STAGE_DIR}/${PTI_MODULE_SUBDIR}")

    add_custom_target(
      lib_load_stage
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PTI_LIB_LOAD_PRELOAD_CORE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:pti_view>"
              "${PTI_LIB_LOAD_PRELOAD_STAGE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:pti>"
              "${PTI_LIB_LOAD_PRELOAD_CORE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Xpti::xpti>"
              "${PTI_LIB_LOAD_PRELOAD_STAGE_DIR}"
      COMMAND ${CMAKE_COMMAND}
              "-DPTI_XPTI_LIBRARY=$<TARGET_FILE:Xpti::xpti>"
              "-DPTI_XPTI_RUNTIME_DIRECTORY=$<TARGET_FILE_DIR:Xpti::xpti>"
              "-DPTI_PRELOAD_STAGE_DIRECTORY=${PTI_LIB_LOAD_PRELOAD_STAGE_DIR}"
              -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stage_xpti_runtime_dependencies.cmake"
      DEPENDS pti pti_view
      VERBATIM)

    add_executable(lib_load_suite lib_load_suite.cc)
    add_dependencies(lib_load_suite lib_load_stage)
    target_compile_definitions(
      lib_load_suite
      PRIVATE
        PTI_PRELOAD_VIEW_LIBRARY_PATH="${PTI_LIB_LOAD_PRELOAD_STAGE_DIR}/$<TARGET_FILE_NAME:pti_view>"
        PTI_PRELOAD_CORE_LIBRARY_PATH="${PTI_LIB_LOAD_PRELOAD_CORE_DIR}/$<TARGET_FILE_NAME:pti>")
    # Intentionally does not link against either PTI library.
    target_link_libraries(lib_load_suite PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS})

    gtest_discover_tests(
      lib_load_suite
      DISCOVERY_TIMEOUT 60
      PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH="
        LABELS "unit"
        LABELS "libload"
        LABELS "itt")
  endif()

  add_executable(itt_basic itt_basic.cc)
  target_link_options(itt_basic PUBLIC -fsycl)
  target_compile_options(itt_basic PUBLIC -fsycl)
  target_include_directories(itt_basic PRIVATE "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
                                               "${PROJECT_SOURCE_DIR}/src/utils")
  target_link_libraries(itt_basic PRIVATE Pti::pti_view ittapi::ittnotify ${FS_LIB})
  add_test(NAME itt_basic COMMAND itt_basic)
  set_tests_properties(itt_basic PROPERTIES
    ENVIRONMENT "INTEL_LIBITTNOTIFY64="
    LABELS "functional;itt")

  if (TARGET sycl_allreduce_usm_test)
    find_package(oneCCL 2021.17 QUIET)
    find_package(MPI QUIET)

    if (TARGET oneCCL AND MPI_FOUND)
      add_test(NAME sycl_allreduce_usm_test
              COMMAND ${CMAKE_COMMAND} -E env
                      "INTEL_LIBITTNOTIFY64="
                      ${MPIEXEC_EXECUTABLE} -n 2
                      $<TARGET_FILE:sycl_allreduce_usm_test> gpu device)
      set_tests_properties(sycl_allreduce_usm_test PROPERTIES
        LABELS "functional;ccl;itt")
    endif()
  endif()

  if (TARGET sycl_allreduce_test)
    find_package(oneCCL 2021.17 QUIET)
    find_package(MPI QUIET)

    if (TARGET oneCCL AND MPI_FOUND)
      add_test(NAME sycl_allreduce_test
              COMMAND ${CMAKE_COMMAND} -E env
                      "INTEL_LIBITTNOTIFY64="
                      ${MPIEXEC_EXECUTABLE} -n 2
                      $<TARGET_FILE:sycl_allreduce_test> gpu)
      set_tests_properties(sycl_allreduce_test PROPERTIES
        LABELS "functional;ccl;itt")
    endif()
  endif()

endif()

# Register this after all other libload tests so default serial CTest output shows it last.
if(TARGET libld_origin_test)
  gtest_discover_tests(
    libld_origin_test
    DISCOVERY_TIMEOUT 60
    PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH="
      LABELS "unit"
      LABELS "libload")
endif()

if(PTI_FUZZ)
  add_subdirectory(fuzz)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory(itt_lib_env)
endif()
