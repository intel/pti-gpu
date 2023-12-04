include(FindPackageHandleStandardArgs)
find_library(Xptifw_LIBRARY
    NAMES xptifw
    HINTS
        ENV LD_LIBRARY_PATH
        ENV LIBRARY_PATH
    )
find_path(Xptifw_INCLUDE_DIR
    NAMES xpti/xpti_trace_framework.h
    PATH_SUFFIXES
        include
        ../include
    )

find_package_handle_standard_args(Xptifw REQUIRED_VARS Xptifw_LIBRARY Xptifw_INCLUDE_DIR)

if (Xptifw_FOUND)
    mark_as_advanced(Xptifw_LIBRARY)
    mark_as_advanced(Xptifw_INCLUDE_DIR)
endif()

if (Xptifw_FOUND AND NOT TARGET Xptifw::Xptifw)
  add_library(Xptifw::Xptifw SHARED IMPORTED)
  set_target_properties(
    Xptifw::Xptifw
    PROPERTIES IMPORTED_LOCATION "${Xptifw_LIBRARY}"
               INTERFACE_COMPILE_DEFINITIONS
               "XPTI_API_EXPORTS;XPTI_CALLBACK_API_EXPORTS"
               INTERFACE_INCLUDE_DIRECTORIES "${Xptifw_INCLUDE_DIR}")
endif()
