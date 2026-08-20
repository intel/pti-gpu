# file(GET_RUNTIME_DEPENDENCIES) correctly warns when XPTI's incomplete ELF metadata
# requires the explicit compiler runtime directory. Run the scan in a child CMake process
# so expected diagnostics do not pollute successful builds, but preserve all output on
# failure.
if(NOT PTI_XPTI_RUNTIME_DEPENDENCY_SCAN)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DPTI_XPTI_RUNTIME_DEPENDENCY_SCAN=ON"
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

# Copies the non-system runtime closure of Xpti into a staged interface directory.
# The test runs with LD_LIBRARY_PATH empty, so LoadCoreLibrary must recursively pre-load
# these files through the interface library's RUNPATH.

foreach(required_variable
        PTI_XPTI_LIBRARY
        PTI_XPTI_RUNTIME_DIRECTORY
        PTI_PRELOAD_STAGE_DIRECTORY)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} must be set")
  endif()
endforeach()

file(REAL_PATH "${PTI_XPTI_RUNTIME_DIRECTORY}" PTI_XPTI_RUNTIME_ROOT)
file(GET_RUNTIME_DEPENDENCIES
  LIBRARIES "${PTI_XPTI_LIBRARY}"
  DIRECTORIES "${PTI_XPTI_RUNTIME_ROOT}"
  RESOLVED_DEPENDENCIES_VAR PTI_XPTI_RESOLVED_DEPENDENCIES
  UNRESOLVED_DEPENDENCIES_VAR PTI_XPTI_UNRESOLVED_DEPENDENCIES)

if(PTI_XPTI_UNRESOLVED_DEPENDENCIES)
  list(JOIN PTI_XPTI_UNRESOLVED_DEPENDENCIES ", " unresolved)
  message(FATAL_ERROR "Unable to resolve the XPTI runtime closure: ${unresolved}")
endif()

foreach(dependency IN LISTS PTI_XPTI_RESOLVED_DEPENDENCIES)
  file(REAL_PATH "${dependency}" PTI_XPTI_RESOLVED_DEPENDENCY)
  cmake_path(IS_PREFIX PTI_XPTI_RUNTIME_ROOT "${PTI_XPTI_RESOLVED_DEPENDENCY}" NORMALIZE
             is_xpti_runtime_dependency)
  if(is_xpti_runtime_dependency)
    file(COPY "${dependency}" DESTINATION "${PTI_PRELOAD_STAGE_DIRECTORY}"
         FOLLOW_SYMLINK_CHAIN)
  endif()
endforeach()
