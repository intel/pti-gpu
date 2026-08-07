project(print_options_schema NONE)

# --print-options-schema is HW-independent (it prints and exits before any Level
# Zero init), so this test needs no GPU and no application. It invokes the built
# unitrace binary directly -- not via run_test.py, which always appends an app
# and diffs its output -- and lets check_options_schema.py validate the JSON schema.
# The binary is unitrace.exe on Windows, matching run_test.py's resolution.
if(WIN32)
  set(UNITRACE_BINARY ${CMAKE_SOURCE_DIR}/../../build/unitrace.exe)
else()
  set(UNITRACE_BINARY ${CMAKE_SOURCE_DIR}/../../build/unitrace)
endif()
add_test(
  NAME print_options_schema
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_options_schema.py
          ${UNITRACE_BINARY})
