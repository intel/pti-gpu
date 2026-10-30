if("${ITT_BASIC_EXECUTABLE}" STREQUAL "")
  message(FATAL_ERROR "ITT_BASIC_EXECUTABLE must be set")
endif()

set(test_environment "PTILOG_LEVEL=warn")
if(ITT_COLLECTOR_PATH)
  if(EXISTS "${ITT_COLLECTOR_PATH}")
    message(FATAL_ERROR "The test requires a nonexistent collector: ${ITT_COLLECTOR_PATH}")
  endif()
  list(APPEND test_environment
    "INTEL_LIBITTNOTIFY64=${ITT_COLLECTOR_PATH}"
    "PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR=1")
else()
  list(APPEND test_environment
    "--unset=INTEL_LIBITTNOTIFY64"
    "--unset=PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment} "${ITT_BASIC_EXECUTABLE}"
  RESULT_VARIABLE test_result
  OUTPUT_VARIABLE test_output
  ERROR_VARIABLE test_output
  TIMEOUT 55)
message("${test_output}")
if(NOT "${test_result}" STREQUAL "0")
  message(FATAL_ERROR "itt_basic failed: ${test_result}")
endif()

string(FIND "${test_output}" "BEFORE_COMMUNICATION_ENABLE" before_enable)
string(FIND "${test_output}" "AFTER_COMMUNICATION_ENABLE" after_enable)
if(before_enable LESS 0 OR after_enable LESS_EQUAL before_enable)
  message(FATAL_ERROR "Missing or out-of-order communication enable markers")
endif()

set(warning_prefix "Cannot enable communication tracing:")
string(FIND "${test_output}" "${warning_prefix}" first_warning)
if(NOT ITT_COLLECTOR_PATH)
  if(NOT first_warning EQUAL -1)
    message(FATAL_ERROR "Self-registration unexpectedly emitted a collector-unavailable warning")
  endif()
  return()
endif()

set(expected_warning
  "${warning_prefix} ITT collector path INTEL_LIBITTNOTIFY64='${ITT_COLLECTOR_PATH}' did not exist;")
string(FIND "${test_output}" "${expected_warning}" warning_position)
if(warning_position EQUAL -1)
  message(FATAL_ERROR "Missing collector warning naming '${ITT_COLLECTOR_PATH}'")
endif()

string(FIND "${test_output}" "${warning_prefix}" last_warning REVERSE)
if(first_warning LESS_EQUAL before_enable OR last_warning GREATER_EQUAL after_enable)
  message(FATAL_ERROR "Collector warning must appear only between the ptiViewEnable markers")
endif()
