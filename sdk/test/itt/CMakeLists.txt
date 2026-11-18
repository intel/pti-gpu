#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# oneCCL ITT collection and domain-filtering tests.
if(HAVE_SYCL)
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

  # Loading the core and registering callbacks persist for the process lifetime.
  # Keep this startup check in its own executable so every run starts fresh.
  add_executable(itt_initialization_test itt_initialization_test.cc)
  target_compile_definitions(itt_initialization_test PRIVATE
    PTI_TEST_CORE_LIBRARY_PATH="$<TARGET_FILE:pti>")
  target_link_libraries(itt_initialization_test PRIVATE
    Pti::pti_view ittapi::ittnotify ${CMAKE_DL_LIBS})
  add_dependencies(itt_initialization_test pti)
  add_test(
    NAME IttTest.IttSetupDoesNotLoadCoreBeforeEnable
    COMMAND itt_initialization_test)
  set_tests_properties(IttTest.IttSetupDoesNotLoadCoreBeforeEnable PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>"
    LABELS "unit;itt")

  # Minimal external ITT collector used by
  # IttTest.ExternalTrivialCollectorDisablesPtiTracing.
  add_library(itt_trivial_collector SHARED itt_trivial_collector.cc)
  target_link_libraries(itt_trivial_collector PRIVATE
    ittapi::headers
    spdlog::spdlog)
  target_compile_definitions(itt_trivial_collector PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
  add_dependencies(itt_test_suite itt_trivial_collector pti)

  # These cases run through the explicit CTest scenarios below, which supply
  # their startup environments. Exclude duplicate registrations when discovery
  # filtering is supported; older CMake versions retain the existing behavior.
  set(PTI_ITT_DISCOVERY_FILTER_ARGS)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.22")
    string(CONCAT PTI_ITT_SCENARIO_FILTER
      "-IttTest.PreconfiguredPtiCollectorEnablesPtiTracing:"
      "IttTest.ExternalTrivialCollectorDisablesPtiTracing:"
      "IttTest.UnavailableCollectorRejectsCommunicationView")
    set(PTI_ITT_DISCOVERY_FILTER_ARGS TEST_FILTER "${PTI_ITT_SCENARIO_FILTER}")
  endif()

  # Run each collector test with libpti_view.so preconfigured. Pass labels
  # separately because gtest_discover_tests does not preserve a semicolon list.
  gtest_discover_tests(
     itt_test_suite
     ${PTI_ITT_DISCOVERY_FILTER_ARGS}
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

  # Preserve INTEL_LIBITTNOTIFY64 when it already points to libpti_view.so and
  # verify that PTI tracing remains enabled.
  add_test(
    NAME itt_preconfigured_pti_collector_enables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.PreconfiguredPtiCollectorEnablesPtiTracing)
  set_tests_properties(itt_preconfigured_pti_collector_enables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:Pti::pti_view>;PTI_TEST_EXPECT_PRECONFIGURED_PTI_COLLECTOR=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # PTI's SONAME must identify the interface already mapped into this process.
  add_test(
    NAME itt_preconfigured_pti_library_name_enables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.PreconfiguredPtiCollectorEnablesPtiTracing)
  set_tests_properties(itt_preconfigured_pti_library_name_enables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_SONAME_FILE_NAME:Pti::pti_view>;PTI_TEST_EXPECT_PRECONFIGURED_PTI_COLLECTOR=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # Preserve an externally configured collector: itt_trivial_collector receives
  # the ITT calls and PTI tracing remains disabled.
  add_test(
    NAME itt_external_trivial_collector_disables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.ExternalTrivialCollectorDisablesPtiTracing)
  set_tests_properties(itt_external_trivial_collector_disables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE:itt_trivial_collector>;PTI_TEST_EXPECT_NO_ITT_COLLECTION=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # Match unitrace's preloaded collector selected through a bare library name.
  add_test(
    NAME itt_preloaded_external_library_name_disables_pti_tracing
    COMMAND itt_test_suite --gtest_filter=IttTest.ExternalTrivialCollectorDisablesPtiTracing)
  set_tests_properties(itt_preloaded_external_library_name_disables_pti_tracing PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=$<TARGET_FILE_NAME:itt_trivial_collector>;LD_PRELOAD=$<TARGET_FILE:itt_trivial_collector>;PTI_TEST_EXPECT_NO_ITT_COLLECTION=1"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  add_test(
    NAME itt_unresolved_library_name_rejects_communication_view
    COMMAND itt_test_suite --gtest_filter=IttTest.UnavailableCollectorRejectsCommunicationView)
  set_tests_properties(itt_unresolved_library_name_rejects_communication_view PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=libpti_test_missing_itt_collector.so;PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR=1;PTI_TEST_PTI_VIEW_PATH=$<TARGET_FILE:Pti::pti_view>"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  add_test(
    NAME itt_missing_collector_rejects_communication_view
    COMMAND itt_test_suite --gtest_filter=IttTest.UnavailableCollectorRejectsCommunicationView)
  set_tests_properties(itt_missing_collector_rejects_communication_view PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=${PROJECT_BINARY_DIR}/missing_itt_collector.so;PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR=1;PTI_TEST_PTI_VIEW_PATH=$<TARGET_FILE:Pti::pti_view>"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # A component longer than Linux's NAME_MAX makes the path lookup fail.
  string(RANDOM LENGTH 256 ALPHABET "x" PTI_ITT_INVALID_PATH_COMPONENT)
  add_test(
    NAME itt_configuration_failure_rejects_communication_view
    COMMAND itt_test_suite --gtest_filter=IttTest.UnavailableCollectorRejectsCommunicationView)
  set_tests_properties(itt_configuration_failure_rejects_communication_view PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "INTEL_LIBITTNOTIFY64=${PROJECT_BINARY_DIR}/${PTI_ITT_INVALID_PATH_COMPONENT};PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR=1;PTI_TEST_PTI_VIEW_PATH=$<TARGET_FILE:Pti::pti_view>"
    FAIL_REGULAR_EXPRESSION "did not match any test;\\[  SKIPPED \\]"
    LABELS "unit;itt")

  # Add linked and runtime-loaded variants of the oneCCL domain-filtering tests.
  add_subdirectory(ccl_shared_libs)

  # Functional smoke test: self-register libpti_view.so and collect three ITT
  # records from basic and nested oneCCL tasks.
  add_executable(itt_basic itt_basic.cc)
  target_link_options(itt_basic PUBLIC -fsycl)
  target_compile_options(itt_basic PUBLIC -fsycl)
  target_include_directories(itt_basic PRIVATE "${PROJECT_SOURCE_DIR}/samples/samples_utilities"
                                               "${PROJECT_SOURCE_DIR}/src/utils")
  target_link_libraries(itt_basic PRIVATE Pti::pti_view ittapi::ittnotify ${FS_LIB})
  add_test(NAME itt_basic
    COMMAND ${CMAKE_COMMAND}
            "-DITT_BASIC_EXECUTABLE=$<TARGET_FILE:itt_basic>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_itt_basic.cmake")
  add_test(NAME itt_basic_missing_collector
    COMMAND ${CMAKE_COMMAND}
            "-DITT_BASIC_EXECUTABLE=$<TARGET_FILE:itt_basic>"
            "-DITT_COLLECTOR_PATH=${PROJECT_BINARY_DIR}/kykyky"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_itt_basic.cmake")
  set_tests_properties(itt_basic itt_basic_missing_collector PROPERTIES
    TIMEOUT 60
    LABELS "functional;itt")

  if (NOT DEFINED PTI_CCL_TEST_SKIP_RETURN_CODE)
    # Match the sample exit code in samples/itt_ccl/CMakeLists.txt.
    set(PTI_CCL_TEST_SKIP_RETURN_CODE 77)
  endif()

  # Exercise self-registration with real oneCCL workloads. Keeping
  # INTEL_LIBITTNOTIFY64 empty verifies registration before oneCCL's first ITT call.
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
        SKIP_RETURN_CODE "${PTI_CCL_TEST_SKIP_RETURN_CODE}"
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
        SKIP_RETURN_CODE "${PTI_CCL_TEST_SKIP_RETURN_CODE}"
        LABELS "functional;ccl;itt")
    endif()
  endif()
endif()

# Collector configuration tests also run when SYCL is unavailable.
add_subdirectory(lib_env)
