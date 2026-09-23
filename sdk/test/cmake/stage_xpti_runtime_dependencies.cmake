# file(GET_RUNTIME_DEPENDENCIES) correctly warns when resolving Core's dependency graph
# requires the explicit compiler runtime directory. Run the scan in a child CMake process
# so expected diagnostics do not pollute successful builds, but preserve all output on
# failure.
if(NOT PTI_XPTI_RUNTIME_DEPENDENCY_SCAN)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DPTI_XPTI_RUNTIME_DEPENDENCY_SCAN=ON"
            "-DPTI_CORE_LIBRARY=${PTI_CORE_LIBRARY}"
            "-DPTI_XPTI_LIBRARY=${PTI_XPTI_LIBRARY}"
            "-DPTI_XPTI_RUNTIME_DIRECTORY=${PTI_XPTI_RUNTIME_DIRECTORY}"
            "-DPTI_PRELOAD_STAGE_DIRECTORY=${PTI_PRELOAD_STAGE_DIRECTORY}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE stage_result
    OUTPUT_VARIABLE stage_stdout
    ERROR_VARIABLE stage_stderr)
  if(NOT stage_result EQUAL 0)
    message(FATAL_ERROR "Failed to stage the XPTI runtime closure:\n${stage_stdout}${stage_stderr}")
  endif()
  return()
endif()

# Copies XPTI and Core's non-system compiler runtime closure into a staged interface directory.
# The test runs with LD_LIBRARY_PATH empty, so LoadCoreLibrary must recursively pre-load
# these files through the interface library's RUNPATH.

foreach(required_variable
        PTI_CORE_LIBRARY
        PTI_XPTI_LIBRARY
        PTI_XPTI_RUNTIME_DIRECTORY
        PTI_PRELOAD_STAGE_DIRECTORY)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} must be set")
  endif()
endforeach()

file(COPY "${PTI_XPTI_LIBRARY}" DESTINATION "${PTI_PRELOAD_STAGE_DIRECTORY}"
     FOLLOW_SYMLINK_CHAIN)

get_filename_component(PTI_XPTI_RUNTIME_ROOT "${PTI_XPTI_RUNTIME_DIRECTORY}" REALPATH)
file(GET_RUNTIME_DEPENDENCIES
  LIBRARIES "${PTI_CORE_LIBRARY}" "${PTI_XPTI_LIBRARY}"
  DIRECTORIES "${PTI_XPTI_RUNTIME_ROOT}"
  RESOLVED_DEPENDENCIES_VAR PTI_XPTI_RESOLVED_DEPENDENCIES
  UNRESOLVED_DEPENDENCIES_VAR PTI_XPTI_UNRESOLVED_DEPENDENCIES)

if(PTI_XPTI_UNRESOLVED_DEPENDENCIES)
  list(JOIN PTI_XPTI_UNRESOLVED_DEPENDENCIES ", " unresolved)
  message(FATAL_ERROR "Unable to resolve the XPTI runtime closure: ${unresolved}")
endif()

foreach(dependency IN LISTS PTI_XPTI_RESOLVED_DEPENDENCIES)
  get_filename_component(PTI_XPTI_RESOLVED_DEPENDENCY "${dependency}" REALPATH)
  file(RELATIVE_PATH PTI_XPTI_RUNTIME_RELATIVE_DEPENDENCY
       "${PTI_XPTI_RUNTIME_ROOT}" "${PTI_XPTI_RESOLVED_DEPENDENCY}")
  if(NOT PTI_XPTI_RUNTIME_RELATIVE_DEPENDENCY MATCHES "^\\.\\.(/|$)")
    file(COPY "${dependency}" DESTINATION "${PTI_PRELOAD_STAGE_DIRECTORY}"
         FOLLOW_SYMLINK_CHAIN)
  endif()
endforeach()
