project(internal_conditional_trace CXX)

add_executable(internal_conditional_trace conditional.cc)
add_sycl_to_target(internal_conditional_trace)

# Reuse the ITT headers and static library produced by the main unitrace build,
# so the workload can emit __itt_resume / __itt_pause / __itt_detach.
set(UNITRACE_BUILD_DIR "${CMAKE_SOURCE_DIR}/../../build")

target_include_directories(internal_conditional_trace PRIVATE "${UNITRACE_BUILD_DIR}/ittapi/include")

# The ittnotify static library name/location differs by platform:
#   Linux:   ${UNITRACE_BUILD_DIR}/libittnotify.a   (also ittapi/bin/libittnotify.a)
#   Windows: ${UNITRACE_BUILD_DIR}/ittapi/bin/libittnotify.lib
if(WIN32 AND EXISTS "${UNITRACE_BUILD_DIR}/ittapi/bin/libittnotify.lib")
  set(ITTNOTIFY_LIB "${UNITRACE_BUILD_DIR}/ittapi/bin/libittnotify.lib")
elseif(EXISTS "${UNITRACE_BUILD_DIR}/libittnotify.a")
  set(ITTNOTIFY_LIB "${UNITRACE_BUILD_DIR}/libittnotify.a")
elseif(EXISTS "${UNITRACE_BUILD_DIR}/ittapi/bin/libittnotify.a")
  set(ITTNOTIFY_LIB "${UNITRACE_BUILD_DIR}/ittapi/bin/libittnotify.a")
else()
  message(FATAL_ERROR "internal_conditional_trace: ittnotify library not found under ${UNITRACE_BUILD_DIR}")
endif()
target_link_libraries(internal_conditional_trace PRIVATE "${ITTNOTIFY_LIB}")

if(NOT WIN32)
  target_link_libraries(internal_conditional_trace PRIVATE dl)
endif()

# Env-var control path (PTI_ENABLE_COLLECTION = 1/0/-1): works on Linux AND
# Windows. The "env" argument selects this path; -t points the gold lookup at
# internal_conditional_trace so it reuses the same gold file.
#add_test(NAME internal_conditional_trace_env COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../run_test.py -c ${CMAKE_SOURCE_DIR} -t internal_conditional_trace internal_conditional_trace env)

# ITT-API control path (__itt_resume/pause/detach): works on Linux AND Windows.
# unitrace_tool.dll exports its __itt_* collector symbols (see
# WINDOWS_EXPORT_ALL_SYMBOLS in the main CMakeLists), so the statically-linked
# ittnotify in the app resolves the collector and the control takes effect.
add_test(NAME internal_conditional_trace COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../run_test.py -c ${CMAKE_SOURCE_DIR} internal_conditional_trace GPU)
