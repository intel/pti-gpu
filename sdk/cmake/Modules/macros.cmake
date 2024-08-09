set(PTI_CMAKE_MACRO_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. CACHE INTERNAL "")

macro(SetRequiredCMakeVersion)
  set(REQUIRED_CMAKE_VERSION 3.12)
endmacro()

macro(RequirePythonInterp)
  if(NOT DEFINED _Python_EXECUTABLE)
    find_package(Python COMPONENTS Interpreter REQUIRED)
    set(PYTHON_EXECUTABLE "${Python_EXECUTABLE}")
  else()
    set(PYTHON_EXECUTABLE "${_Python_EXECUTABLE}")
  endif()
endmacro()

macro(SetCompilerFlags)
  file(STRINGS "${PTI_CMAKE_MACRO_DIR}/VERSION" PTI_VERSION)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPTI_VERSION=${PTI_VERSION}")
endmacro()

macro(SetBuildType)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
  endif()
  message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Bitness: 64 bits")
  else()
    message(FATAL_ERROR "32-bit mode is not supported")
  endif()
endmacro()

macro(FindOpenCLLibrary TARGET)
  if(WIN32)
    if(NOT OpenCL_FOUND)
      find_package(OpenCL 2.1)
    endif()
  else()
    if(DEFINED ENV{LD_LIBRARY_PATH})
      string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
    endif()

    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .so.1)
    find_library(OpenCL_LIBRARY
      NAMES OpenCL
      PATHS ${SEARCH_LIB_PATH})
  endif()

  if(EXISTS ${OpenCL_LIBRARY})
    message(STATUS
      "OpenCL library is found at ${OpenCL_LIBRARY}")
    target_link_libraries (${TARGET} ${OpenCL_LIBRARY})
  else()
    message(FATAL_ERROR
      "OpenCL library is not found. "
      "You may need to install OpenCL ICD Loader or specify the path "
      "to the library as -DCMAKE_LIBRARY_PATH=<OpenCL_Library_Path>")
  endif()
endmacro()

macro(FindOpenCLHeaders TARGET)
  if(WIN32)
    if(NOT OpenCL_FOUND)
      find_package(OpenCL 2.1)
    endif()
  else()
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(CL/cl.h OpenCL_INCLUDE_DIRS)
    if (OpenCL_INCLUDE_DIRS)
      include(CheckTypeSize)
      set(CMAKE_EXTRA_INCLUDE_FILES "CL/cl.h")
      CHECK_TYPE_SIZE(cl_kernel_sub_group_info CL_KERNEL_SUB_GROUP_INFO_SIZE LANGUAGE CXX)
      set(CMAKE_EXTRA_INCLUDE_FILES)
      if(NOT CL_KERNEL_SUB_GROUP_INFO_SIZE)
        set(OpenCL_INCLUDE_DIRS 0)
      endif()
    endif()
  endif()

  if(NOT OpenCL_INCLUDE_DIRS)
    RequirePythonInterp()
    set(OPENCL_INC_PATH "${CMAKE_BINARY_DIR}")

    message(STATUS "OpenCL headers are not found, will be downloaded automatically")

    add_custom_target(cl_headers ALL
                      DEPENDS "${OPENCL_INC_PATH}/CL/cl.h"
                              "${OPENCL_INC_PATH}/CL/cl_gl.h"
                              "${OPENCL_INC_PATH}/CL/cl_version.h"
                              "${OPENCL_INC_PATH}/CL/cl_platform.h")
    add_custom_command(OUTPUT "${OPENCL_INC_PATH}/CL/cl.h"
                              "${OPENCL_INC_PATH}/CL/cl_gl.h"
                              "${OPENCL_INC_PATH}/CL/cl_version.h"
                              "${OPENCL_INC_PATH}/CL/cl_platform.h"
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_cl_headers.py" "${OPENCL_INC_PATH}" "${CMAKE_BINARY_DIR}")

    target_include_directories(${TARGET} PUBLIC "${OPENCL_INC_PATH}")
    add_dependencies(${TARGET} cl_headers)
  else()
    target_include_directories (${TARGET} PUBLIC ${OpenCL_INCLUDE_DIRS})
  endif()
  target_compile_options(${TARGET}
    PRIVATE -DCL_TARGET_OPENCL_VERSION=210)
endmacro()

macro(GetOpenCLTracingHeaders TARGET)
  set(OPENCL_TRACING_INC_PATH "${CMAKE_BINARY_DIR}")
  RequirePythonInterp()

  add_custom_target(cl_tracing_headers ALL
                    DEPENDS ${OPENCL_TRACING_INC_PATH}/CL/tracing_api.h
                            ${OPENCL_TRACING_INC_PATH}/CL/tracing_types.h
                            ${OPENCL_TRACING_INC_PATH}/CL/cl_ext_private.h)
  add_custom_command(OUTPUT ${OPENCL_TRACING_INC_PATH}/CL/tracing_api.h
                            ${OPENCL_TRACING_INC_PATH}/CL/tracing_types.h
                            ${OPENCL_TRACING_INC_PATH}/CL/cl_ext_private.h
                    COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_cl_tracing_headers.py" ${OPENCL_TRACING_INC_PATH} ${CMAKE_BINARY_DIR})

  target_include_directories(${TARGET}
    PUBLIC "${OPENCL_TRACING_INC_PATH}")
  add_dependencies(${TARGET}
    cl_tracing_headers)
endmacro()

macro(GetITT TARGET)
  set(ITT_INC_PATH "${CMAKE_BINARY_DIR}")
  RequirePythonInterp()

  add_custom_target(itt_headers ALL
                    DEPENDS ${ITT_INC_PATH}/ITT/disable_warnings.h
                            ${ITT_INC_PATH}/ITT/ittnotify_config.h
                            ${ITT_INC_PATH}/ITT/ittnotify_static.c
                            ${ITT_INC_PATH}/ITT/ittnotify_static.h
                            ${ITT_INC_PATH}/ITT/ittnotify_types.h
                            ${ITT_INC_PATH}/ITT/ittnotify.h
                            ${ITT_INC_PATH}/ITT/legacy/ittnotify.h)
  add_custom_command(OUTPUT ${ITT_INC_PATH}/ITT/disable_warnings.h
                            ${ITT_INC_PATH}/ITT/ittnotify_config.h
                            ${ITT_INC_PATH}/ITT/ittnotify_static.c
                            ${ITT_INC_PATH}/ITT/ittnotify_static.h
                            ${ITT_INC_PATH}/ITT/ittnotify_types.h
                            ${ITT_INC_PATH}/ITT/ittnotify.h
                            ${ITT_INC_PATH}/ITT/legacy/ittnotify.h
                    COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_itt.py" ${ITT_INC_PATH} ${CMAKE_BINARY_DIR})

  target_include_directories(${TARGET}
    PUBLIC "${ITT_INC_PATH}")
  add_dependencies(${TARGET}
    itt_headers)
endmacro()

macro(FindIGALibrary TARGET)
  if(WIN32)
    RequirePythonInterp()

    foreach(VALUE IN ITEMS $ENV{PATH})
      if(VALUE MATCHES "[S|s]ystem32*$")
        set(WIN_SYS_PATH ${VALUE})
        break()
      endif()
    endforeach()
    file(GLOB_RECURSE IGA_DLL_PATH "${WIN_SYS_PATH}/iga64.dll")
    if(NOT IGA_DLL_PATH)
      message(FATAL_ERROR
        "IGA is not found. "
        "You may need to install Intel(R) Processor Graphics Driver to fix this issue.")
    else()
      list(GET IGA_DLL_PATH 0 IGA_DLL_PATH)
      message(STATUS
        "IGA is found at ${IGA_DLL_PATH}")
      find_library(IGA_LIB_PATH
        NAMES iga64
        PATHS ${SEARCH_LIB_PATH})
      if(NOT IGA_LIB_PATH)
        add_custom_target(iga_lib ALL
                          DEPENDS ${CMAKE_BINARY_DIR}/iga64.lib)
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/iga64.lib
                           COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/convert_dll_to_lib.py" ${CMAKE_BINARY_DIR} ${IGA_DLL_PATH})
        add_dependencies(${TARGET} iga_lib)
        add_custom_command(TARGET ${TARGET} POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different "${IGA_DLL_PATH}" ${CMAKE_BINARY_DIR}/iga64.dll
                           COMMAND ${CMAKE_COMMAND} -E echo "File ${IGA_DLL_PATH} was copied into project binary directory: ${CMAKE_BINARY_DIR}")
        target_link_libraries(${TARGET} "${CMAKE_BINARY_DIR}/iga64.lib")
      else()
        message(STATUS
          "IGA library is found at ${IGA_LIB_PATH}")
        target_link_libraries(${TARGET}
          "${IGA_LIB_PATH}")
      endif()
    endif()
  else()
    if(DEFINED ENV{LD_LIBRARY_PATH})
      string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
    endif()
    find_library(IGA_LIB_PATH
      NAMES iga64
      PATHS ${SEARCH_LIB_PATH})
    if(NOT IGA_LIB_PATH)
      message(FATAL_ERROR
        "IGA library is not found. "
        "You may need to install Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver to fix this issue.")
    else()
      message(STATUS
        "IGA library is found at ${IGA_LIB_PATH}")
    endif()
  target_link_libraries(${TARGET}
    "${IGA_LIB_PATH}")
  endif()
endmacro()

macro(GetIGAHeaders TARGET)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(iga/iga.h IGA_H_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/iga_types_ext.hpp IGA_TYPES_EXT_HPP_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/iga_types_swsb.hpp IGA_TYPES_SWSB_HPP_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/iga_bxml_ops.hpp IGA_BXML_OPS_HPP_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/iga_bxml_enums.hpp IGA_BXML_ENUMS_HPP_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/kv.h KV_H_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(iga/kv.hpp KV_HPP_INC_FOUND)
  if (NOT (IGA_H_INC_FOUND AND
           IGA_TYPES_EXT_HPP_INC_FOUND AND
           IGA_TYPES_SWSB_HPP_INC_FOUND AND
           IGA_BXML_OPS_HPP_INC_FOUND AND
           IGA_BXML_ENUMS_HPP_INC_FOUND AND
           KV_H_INC_FOUND AND
           KV_HPP_INC_FOUND))

    set(IGA_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(iga_headers ALL
                      DEPENDS ${IGA_INC_PATH}/iga/iga.h
                              ${IGA_INC_PATH}/iga/iga_types_ext.hpp
                              ${IGA_INC_PATH}/iga/iga_types_swsb.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_ops.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_enums.hpp
                              ${IGA_INC_PATH}/iga/kv.h
                              ${IGA_INC_PATH}/iga/kv.hpp)
    add_custom_command(OUTPUT ${IGA_INC_PATH}/iga/iga.h
                              ${IGA_INC_PATH}/iga/iga_types_ext.hpp
                              ${IGA_INC_PATH}/iga/iga_types_swsb.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_ops.hpp
                              ${IGA_INC_PATH}/iga/iga_bxml_enums.hpp
                              ${IGA_INC_PATH}/iga/kv.h
                              ${IGA_INC_PATH}/iga/kv.hpp
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_iga_headers.py" ${IGA_INC_PATH} ${CMAKE_BINARY_DIR})

    target_include_directories(${TARGET}
      PUBLIC "${IGA_INC_PATH}")
    add_dependencies(${TARGET}
      iga_headers)

  endif()
endmacro()

macro(GetIGCHeaders TARGET)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(igc/ocl_igc_shared/executable_format/program_debug_data.h PROGRAM_DEBUG_DATA_H_INC_FOUND)
  CHECK_INCLUDE_FILE_CXX(igc/ocl_igc_shared/executable_format/patch_list.h PATCH_LIST_H_INC_FOUND)
  if (NOT (PROGRAM_DEBUG_DATA_H_INC_FOUND AND PATCH_LIST_H_INC_FOUND))

    set(IGC_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(igc_headers ALL
                      DEPENDS ${IGC_INC_PATH}/igc/ocl_igc_shared/executable_format/program_debug_data.h
                              ${IGC_INC_PATH}/igc/ocl_igc_shared/executable_format/patch_list.h)
    add_custom_command(OUTPUT ${IGC_INC_PATH}/igc/ocl_igc_shared/executable_format/program_debug_data.h
                              ${IGC_INC_PATH}/igc/ocl_igc_shared/executable_format/patch_list.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_igc_headers.py" ${IGC_INC_PATH} ${CMAKE_BINARY_DIR})

    target_include_directories(${TARGET}
      PUBLIC "${IGC_INC_PATH}")
    add_dependencies(${TARGET}
      igc_headers)

  endif()
endmacro()

macro(GetGmmHeaders TARGET)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(igdgmm/inc/common/igfxfmid.h IGFXFMID_H_INC_FOUND)
  if (NOT (IGFXFMID_H_INC_FOUND))

    set(GMM_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(arch_header ALL
                      DEPENDS ${GMM_INC_PATH}/igdgmm/inc/common/igfxfmid.h)
    add_custom_command(OUTPUT ${GMM_INC_PATH}/igdgmm/inc/common/igfxfmid.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gmm_headers.py" ${GMM_INC_PATH} ${CMAKE_BINARY_DIR})

    target_include_directories(${TARGET}
      PUBLIC "${GMM_INC_PATH}")
    add_dependencies(${TARGET}
      arch_header)

  endif()
endmacro()

macro(CheckForMDLibrary TARGET)
  if(WIN32)
    foreach(VALUE IN ITEMS $ENV{PATH})
      if(VALUE MATCHES "[S|s]ystem32*$")
        set(WIN_SYS_PATH ${VALUE})
        break()
      endif()
    endforeach()
    file(GLOB_RECURSE MD_LIB_PATH "${WIN_SYS_PATH}/igdmd64.dll")
    if(NOT MD_LIB_PATH)
      message(WARNING
        "Intel(R) Metrics Discovery Application Programming Interface is not found. "
        "You may need to install Intel(R) Processor Graphics Driver to fix this issue.")
    else()
      message(STATUS
        "Intel(R) Metrics Discovery Application Programming Interface is found at ${MD_LIB_PATH}")
      add_custom_command(TARGET ${TARGET} POST_BUILD
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MD_LIB_PATH}" ${CMAKE_BINARY_DIR}/igdmd64.dll
                         COMMAND ${CMAKE_COMMAND} -E echo "File ${MD_LIB_PATH} was copied into project binary directory: ${CMAKE_BINARY_DIR}")
    endif()
  else()
    if(DEFINED ENV{LD_LIBRARY_PATH})
      string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
    endif()
    find_library(MD_LIB_PATH
      NAMES md
      PATHS ${SEARCH_LIB_PATH})
    if(NOT MD_LIB_PATH)
      message(WARNING
        "Intel(R) Metrics Discovery Application Programming Interface library is not found. "
        "You may need to install it to fix this issue.")
    else()
      message(STATUS
        "Intel(R) Metrics Discovery Application Programming Interface library is found at ${MD_LIB_PATH}")
    endif()
  endif()

endmacro()

macro(CheckForMetricsLibrary)
  if(WIN32)
    foreach(VALUE IN ITEMS $ENV{PATH})
      if(VALUE MATCHES "[S|s]ystem32*$")
        set(WIN_SYS_PATH ${VALUE})
        break()
      endif()
    endforeach()
    file(GLOB_RECURSE ML_LIB_PATH "${WIN_SYS_PATH}/igdml64.dll")
  else()
    if(DEFINED ENV{LD_LIBRARY_PATH})
      string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
    endif()
    find_library(ML_LIB_PATH
      NAMES igdml64
      PATHS ${SEARCH_LIB_PATH})
  endif()

  if(NOT ML_LIB_PATH)
    message(WARNING
      "Metrics Library for Metrics Discovery API (Metrics Library for MD API) is not found. "
      "You may need to install Metrics Library for Metrics Discovery API (Metrics Library for MD API) to fix this issue.")
  else()
    message(STATUS
      "Metrics Library for Metrics Discovery API (Metrics Library for MD API) is found at ${ML_LIB_PATH}")
  endif()
endmacro()

macro(GetMDHeaders TARGET)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(metrics_discovery_api.h MDAPI_H_INC_FOUND)
  if (NOT (MDAPI_H_INC_FOUND))
    set(MD_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(md_headers ALL
                      DEPENDS ${MD_INC_PATH}/metrics_discovery_api.h)
    add_custom_command(OUTPUT ${MD_INC_PATH}/metrics_discovery_api.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_md_headers.py" ${MD_INC_PATH} ${CMAKE_BINARY_DIR})

    target_include_directories(${TARGET}
      PUBLIC "${MD_INC_PATH}")
    add_dependencies(${TARGET}
      md_headers)
  endif()
endmacro()

macro(FindGTPinLibrary_legacy TARGET)
  if(DEFINED GTPIN_PATH)
    find_library(GTPIN_LIB_PATH
      NAMES gtpin
      HINTS "${GTPIN_PATH}/Lib/intel64")
    if(GTPIN_LIB_PATH)
      message(STATUS "Graphics Technology Pin (GT Pin) library path: ${GTPIN_LIB_PATH}")
      target_link_libraries(${TARGET}
        ${GTPIN_LIB_PATH})
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) library was not found at ${GTPIN_PATH}/Lib/intel64")
    endif()
  else()
    message(STATUS "Graphics Technology Pin (GT Pin) path was not defined - it will be downloaded automatically on build")
    set(GTPIN_LIB_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()
    if(UNIX)
      add_custom_target(gtpin_libs ALL
                        DEPENDS ${GTPIN_LIB_PATH}/GTPIN/libgcc_s.so.1
                                ${GTPIN_LIB_PATH}/GTPIN/libged.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin_core.so
                                ${GTPIN_LIB_PATH}/GTPIN/libiga_wrapper.so
                                ${GTPIN_LIB_PATH}/GTPIN/libstdc++.so.6)
      add_custom_command(OUTPUT ${GTPIN_LIB_PATH}/GTPIN/libgcc_s.so.1
                                ${GTPIN_LIB_PATH}/GTPIN/libged.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin_core.so
                                ${GTPIN_LIB_PATH}/GTPIN/libiga_wrapper.so
                                ${GTPIN_LIB_PATH}/GTPIN/libstdc++.so.6
                        COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_libs_legacy.py" ${GTPIN_LIB_PATH} ${CMAKE_BINARY_DIR})

      target_link_libraries(${TARGET}
        "${GTPIN_LIB_PATH}/GTPIN/libgtpin.so")
      add_dependencies(${TARGET}
        gtpin_libs)
    elseif(WIN32)
      add_custom_target(gtpin_libs ALL
                        DEPENDS ${GTPIN_LIB_PATH}/GTPIN/gtpin.lib
                                ${GTPIN_LIB_PATH}/ged.dll
                                ${GTPIN_LIB_PATH}/gtpin.dll
                                ${GTPIN_LIB_PATH}/gtpin_core.dll
                                ${GTPIN_LIB_PATH}/iga_wrapper.dll)
      add_custom_command(OUTPUT ${GTPIN_LIB_PATH}/GTPIN/gtpin.lib
                                ${GTPIN_LIB_PATH}/ged.dll
                                ${GTPIN_LIB_PATH}/gtpin.dll
                                ${GTPIN_LIB_PATH}/gtpin_core.dll
                                ${GTPIN_LIB_PATH}/iga_wrapper.dll
                        COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_libs_legacy.py" ${GTPIN_LIB_PATH} ${CMAKE_BINARY_DIR})

      target_link_libraries(${TARGET}
        "${GTPIN_LIB_PATH}/GTPIN/gtpin.lib")
      add_dependencies(${TARGET}
        gtpin_libs)
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) path was not specified. Use -DGTPIN_PATH to set it")
    endif()
  endif()
endmacro()

macro(GetGTPinHeaders_legacy TARGET)
  if(DEFINED GTPIN_PATH)
    message(STATUS "Graphics Technology Pin (GT Pin) include path: ${GTPIN_PATH}/Include")
    target_include_directories(${TARGET}
      PUBLIC "${GTPIN_PATH}/Include"
      PUBLIC "${GTPIN_PATH}/Include/ged/intel64")
  else()
    message(STATUS "Graphics Technology Pin (GT Pin) path was not defined - it will be downloaded automatically on build")
    set(GTPIN_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(gtpin_headers ALL
                      DEPENDS ${GTPIN_INC_PATH}/GTPIN/callbacks.h
                              ${GTPIN_INC_PATH}/GTPIN/client_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/ged_ops.h
                              ${GTPIN_INC_PATH}/GTPIN/clientdb.h
                              ${GTPIN_INC_PATH}/GTPIN/gtpin.h
                              ${GTPIN_INC_PATH}/GTPIN/gtpin.hpp
                              ${GTPIN_INC_PATH}/GTPIN/gtpintool_types.h
                              ${GTPIN_INC_PATH}/GTPIN/init.h
                              ${GTPIN_INC_PATH}/GTPIN/kernel.h
                              ${GTPIN_INC_PATH}/GTPIN/send_exec_semantics.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_basic_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_enumerations.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_enum_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_ins_field.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_arg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_registry.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_core.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gpu_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_utils.h)
    add_custom_command(OUTPUT ${GTPIN_INC_PATH}/GTPIN/callbacks.h
                              ${GTPIN_INC_PATH}/GTPIN/client_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/ged_ops.h
                              ${GTPIN_INC_PATH}/GTPIN/clientdb.h
                              ${GTPIN_INC_PATH}/GTPIN/gtpin.h
                              ${GTPIN_INC_PATH}/GTPIN/gtpin.hpp
                              ${GTPIN_INC_PATH}/GTPIN/gtpintool_types.h
                              ${GTPIN_INC_PATH}/GTPIN/init.h
                              ${GTPIN_INC_PATH}/GTPIN/kernel.h
                              ${GTPIN_INC_PATH}/GTPIN/send_exec_semantics.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_basic_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_enumerations.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_enum_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/intel64/ged_ins_field.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_arg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_registry.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_core.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gpu_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_utils.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_headers_legacy.py" ${GTPIN_INC_PATH} ${CMAKE_BINARY_DIR})

    target_include_directories(${TARGET}
      PUBLIC "${GTPIN_INC_PATH}/GTPIN"
      PUBLIC "${GTPIN_INC_PATH}/GTPIN/ged/intel64")
    add_dependencies(${TARGET}
      gtpin_headers)
  endif()
endmacro()

macro(GetGTPinLibrary TARGET)
  if(NOT DEFINED GTPIN_PATH)
    set(GTPIN_LIB_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()
    if(UNIX)
      add_custom_target(gtpin_libs ALL
                        DEPENDS ${GTPIN_LIB_PATH}/GTPIN/libgcc_s.so.1
                                ${GTPIN_LIB_PATH}/GTPIN/libged.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin_core.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin.so
                                ${GTPIN_LIB_PATH}/GTPIN/libiga_wrapper.so)
      add_custom_command(OUTPUT ${GTPIN_LIB_PATH}/GTPIN/libgcc_s.so.1
                                ${GTPIN_LIB_PATH}/GTPIN/libged.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin_core.so
                                ${GTPIN_LIB_PATH}/GTPIN/libgtpin.so
                                ${GTPIN_LIB_PATH}/GTPIN/libiga_wrapper.so
                        COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_libs.py" ${GTPIN_LIB_PATH} ${CMAKE_BINARY_DIR})
      target_link_libraries(${TARGET}
        "${GTPIN_LIB_PATH}/GTPIN/libgtpin.so")
      target_link_libraries(${TARGET}
        "${GTPIN_LIB_PATH}/GTPIN/libged.so")
      add_dependencies(${TARGET}
        gtpin_libs)
    elseif(WIN32)
      add_custom_target(gtpin_libs ALL
                        DEPENDS ${GTPIN_LIB_PATH}/GTPIN/gtpin.lib
                                ${GTPIN_LIB_PATH}/ged.dll
                                ${GTPIN_LIB_PATH}/gtpin.dll
                                ${GTPIN_LIB_PATH}/gtpin_core.dll
                                ${GTPIN_LIB_PATH}/iga_wrapper.dll)
      add_custom_command(OUTPUT ${GTPIN_LIB_PATH}/GTPIN/gtpin.lib
                                ${GTPIN_LIB_PATH}/ged.dll
                                ${GTPIN_LIB_PATH}/gtpin.dll
                                ${GTPIN_LIB_PATH}/gtpin_core.dll
                                ${GTPIN_LIB_PATH}/iga_wrapper.dll
                        COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_libs.py" ${GTPIN_LIB_PATH} ${CMAKE_BINARY_DIR})
      target_link_libraries(${TARGET}
        "${GTPIN_LIB_PATH}/GTPIN/gtpin.lib")
      add_dependencies(${TARGET}
        gtpin_libs)
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) path was not specified. Use -DGTPIN_PATH to set it")
    endif()
  endif()
endmacro()

macro(FindGTPinLibrary TARGET)
  if(DEFINED GTPIN_PATH)
    find_library(GTPIN_LIB_PATH
      NAMES gtpin
      HINTS "${GTPIN_PATH}/Lib/intel64")
    find_library(GTPIN_GED_LIB_PATH
      NAMES ged
      HINTS "${GTPIN_PATH}/Lib/intel64")
    if(GTPIN_LIB_PATH AND GTPIN_GED_LIB_PATH)
      message(STATUS "Graphics Technology Pin (GT Pin) library path: ${GTPIN_LIB_PATH} & ${GTPIN_GED_LIB_PATH}")
      target_link_libraries(${TARGET}
        ${GTPIN_LIB_PATH})
      target_link_libraries(${TARGET}
        ${GTPIN_GED_LIB_PATH})
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) library was not found at ${GTPIN_PATH}/Lib/intel64")
    endif()
  else()
    message(STATUS "Graphics Technology Pin (GT Pin) path was not defined - it will be downloaded automatically on build")
    GetGTPinLibrary(${TARGET})
  endif()
endmacro()

macro(GetGTPinHeaders TARGET)
  if(NOT DEFINED GTPIN_PATH)
    set(GTPIN_INC_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()

    add_custom_target(gtpin_headers ALL
                      DEPENDS ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_utils.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gen_procedure.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gpu_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_hli_function.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_ins_fields.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gtpin_api.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_profile_array.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_profile_trace.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_reg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_cfg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_core.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_event.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_arch.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_coder.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_procedure.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_hli_library.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_iarg_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_iarg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_ins_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_ins.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_kernel.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_arg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_registry.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_memory_mapper.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_profile_buffer.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_reg_allocator.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_tool.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_vreg_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_vreg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_api_version.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_basic_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_enumerations.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_enum_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_ins_field.h)
    add_custom_command(OUTPUT ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_basic_utils.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gen_procedure.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_gpu_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_hli_function.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_ins_fields.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob_defs.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_knob.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gtpin_api.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_profile_array.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_profile_trace.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_reg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_cfg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_core.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_event.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_arch.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_coder.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_gen_procedure.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_hli_library.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_iarg_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_iarg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_ins_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_ins.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_kernel.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_arg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_knob_registry.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_memory_mapper.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_profile_buffer.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_reg_allocator.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_tool.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_vreg_factory.h
                              ${GTPIN_INC_PATH}/GTPIN/api/igt_vreg.h
                              ${GTPIN_INC_PATH}/GTPIN/api/gt_api_version.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_basic_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_enumerations.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_enum_types.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged.h
                              ${GTPIN_INC_PATH}/GTPIN/ged/ged_ins_field.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_headers.py" ${GTPIN_INC_PATH} ${CMAKE_BINARY_DIR})
    target_include_directories(${TARGET}
      PUBLIC "${GTPIN_INC_PATH}/GTPIN"
      PUBLIC "${GTPIN_INC_PATH}/GTPIN/api"
      PUBLIC "${GTPIN_INC_PATH}/GTPIN/ged/intel64")
    add_dependencies(${TARGET}
      gtpin_headers)
  endif()
endmacro()

macro(FindGTPinHeaders TARGET)
  if(CMAKE_INCLUDE_PATH)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_INCLUDE_PATH})
  endif()
  if(DEFINED GTPIN_PATH)
    message(STATUS "Graphics Technology Pin (GT Pin) include path: ${GTPIN_PATH}/Include, ${GTPIN_PATH}/Include/api, ${GTPIN_PATH}/Include/ged/intel64")
    target_include_directories(${TARGET}
      PUBLIC "${GTPIN_PATH}/Include"
      PUBLIC "${GTPIN_PATH}/Include/api"
      PUBLIC "${GTPIN_PATH}/Include/ged/intel64")
  else()
    message(STATUS "Graphics Technology Pin (GT Pin) path was not defined - it will be downloaded automatically on build")
    GetGTPinHeaders(${TARGET})
  endif()
  set(CMAKE_REQUIRED_INCLUDES)
endmacro()

macro(GetGTPinUtil TARGET UTIL)
  if(NOT DEFINED GTPIN_PATH)
    set(GTPIN_UTIL_PATH "${CMAKE_BINARY_DIR}")
    RequirePythonInterp()
    add_custom_target(gtpin_${UTIL} ALL
                      DEPENDS "${GTPIN_UTIL_PATH}/GTPIN/utils/${UTIL}.h"
                              "${GTPIN_UTIL_PATH}/GTPIN/utils/${UTIL}.cpp")
    add_custom_command(OUTPUT "${GTPIN_UTIL_PATH}/GTPIN/utils/${UTIL}.h"
                              "${GTPIN_UTIL_PATH}/GTPIN/utils/${UTIL}.cpp"
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_gtpin_util.py" ${GTPIN_UTIL_PATH} ${CMAKE_BINARY_DIR} ${UTIL})
    target_sources(${TARGET} PRIVATE
      "${GTPIN_UTIL_PATH}/Examples/utils/${UTIL}.cpp")
    target_include_directories(${TARGET}
      PUBLIC "${GTPIN_UTIL_PATH}/GTPIN/utils")
    add_dependencies(${TARGET}
      gtpin_${UTIL})
  endif()
endmacro()

macro(FindGTPinUtil TARGET UTIL)
  if(DEFINED GTPIN_PATH)
    find_file(GTPIN_${UTIL}_PATH
      NAMES ${UTIL}.cpp
      PATHS "${GTPIN_PATH}/Examples/utils")
    if(GTPIN_${UTIL}_PATH)
      target_sources(${TARGET} PRIVATE
        ${GTPIN_${UTIL}_PATH})
      target_include_directories(${TARGET}
        PUBLIC "${GTPIN_PATH}/Examples/utils")
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) ${UTIL} was not found at ${GTPIN_PATH}/Examples/utils")
    endif()
  else()
    message(STATUS "Graphics Technology Pin (GT Pin) path was not defined - it will be downloaded automatically on build")
    GetGTPinUtil(${TARGET} ${UTIL})
  endif()
endmacro()

macro(GetLevelZeroHeaders TARGET)
  set(L0_INC_PATH "${CMAKE_BINARY_DIR}")
  RequirePythonInterp()

  add_custom_target(level_zero_headers ALL
                    DEPENDS ${L0_INC_PATH}/level_zero/ze_api.h
                            ${L0_INC_PATH}/level_zero/zes_api.h
                            ${L0_INC_PATH}/level_zero/zet_api.h
                            ${L0_INC_PATH}/level_zero/layers/zel_tracing_api.h)
  add_custom_command(OUTPUT ${L0_INC_PATH}/level_zero/ze_api.h
                            ${L0_INC_PATH}/level_zero/zes_api.h
                            ${L0_INC_PATH}/level_zero/zet_api.h
                            ${L0_INC_PATH}/level_zero/layers/zel_tracing_api.h
                    COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_ze_headers.py" ${L0_INC_PATH} ${CMAKE_BINARY_DIR})
  target_include_directories(${TARGET}
    PUBLIC "${L0_INC_PATH}")
  add_dependencies(${TARGET}
    level_zero_headers)
endmacro()

macro(FindL0Library TARGET)
  if(WIN32)
    find_library(L0_LIB_PATH
      NAMES ze_loader
      PATHS ${CMAKE_LIBRARY_PATH})
    if(NOT L0_LIB_PATH)
      foreach(VALUE IN ITEMS $ENV{PATH})
        if(VALUE MATCHES "[S|s]ystem32*$")
          set(WIN_SYS_PATH ${VALUE})
          break()
        endif()
      endforeach()
      file(GLOB_RECURSE L0_DLL_PATH "${WIN_SYS_PATH}/ze_loader.dll")
      if(NOT L0_DLL_PATH)
        message(FATAL_ERROR
          "Level Zero loader is not found. "
          "You may need to install oneAPI Level Zero loader to fix this issue.")
      else()
        message(STATUS
          "Level Zero Loader is found at ${L0_DLL_PATH}")
        find_library(L0_LIB_PATH
          NAMES ze_loader
          PATHS ${SEARCH_LIB_PATH})
        if(NOT L0_LIB_PATH)
          RequirePythonInterp()

          add_custom_target(level_zero_lib ALL
                            DEPENDS ${CMAKE_BINARY_DIR}/ze_loader.lib)
          add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ze_loader.lib
                             COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/convert_dll_to_lib.py" ${CMAKE_BINARY_DIR} ${L0_DLL_PATH})
          add_dependencies(${TARGET} level_zero_lib)
          target_link_libraries(${TARGET} "${CMAKE_BINARY_DIR}/ze_loader.lib")
        else()
          message(STATUS
            "Level Zero library is found at ${L0_LIB_PATH}")
          target_link_libraries(${TARGET}
            "${L0_LIB_PATH}")
        endif()
      endif()
    else()
      message(STATUS
        "Level Zero loader is found at ${L0_LIB_PATH}")
      target_link_libraries(${TARGET}
        "${L0_LIB_PATH}")
    endif()
  else()
    if(DEFINED ENV{LD_LIBRARY_PATH})
      string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
    endif()

    find_library(L0_LIB_PATH
      NAMES ze_loader
      PATHS ${SEARCH_LIB_PATH})
    if(NOT L0_LIB_PATH)
      message(FATAL_ERROR
        "Level Zero loader is not found. "
        "You may need to install oneAPI Level Zero loader to fix this issue.")
    else()
      message(STATUS
        "Level Zero loader is found at ${L0_LIB_PATH}")
    endif()

    get_filename_component(L0_BASE_NAME ${L0_LIB_PATH} NAME)
    target_link_libraries(${TARGET} PUBLIC
      "$<BUILD_INTERFACE:${L0_LIB_PATH}>$<INSTALL_INTERFACE:${L0_BASE_NAME}>")
  endif()
endmacro()

macro(FindL0Headers TARGET)
  if(CMAKE_INCLUDE_PATH)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_INCLUDE_PATH})
  endif()
  include(CheckIncludeFileCXX)
  check_include_file_cxx(level_zero/ze_api.h LO_CORE_INC_FOUND)
  check_include_file_cxx(level_zero/zet_api.h LO_TOOLS_INC_FOUND)
  check_include_file_cxx(level_zero/zes_api.h LO_SYSMAN_INC_FOUND)
  check_include_file_cxx(level_zero/layers/zel_tracing_api.h LO_LAYERS_INC_FOUND)
  if ((NOT LO_CORE_INC_FOUND) OR (NOT LO_TOOLS_INC_FOUND) OR (NOT LO_LAYERS_INC_FOUND) OR (NOT LO_SYSMAN_INC_FOUND))
    message(STATUS "Level Zero headers are not found, will be downloaded automatically")
    GetLevelZeroHeaders(${TARGET})
  endif()
  set(CMAKE_REQUIRED_INCLUDES)
endmacro()

macro(FindL0HeadersPath TARGET L0_GEN_SCRIPT)
  RequirePythonInterp()
  find_path(L0_INC_PATH
    NAMES level_zero
    PATHS ENV CPATH)
  if (NOT L0_INC_PATH)
    message(FATAL_ERROR
      "Level Zero headers path is not found.\n"
      "You may need to install oneAPI Level Zero Driver to fix this issue.")
  else()
    message(STATUS "Level Zero headers are found at ${L0_INC_PATH}")
  endif()

  set(L0_GEN_INC_PATH "${CMAKE_BINARY_DIR}")
  add_custom_target(ze_gen_headers ALL
                    DEPENDS ${L0_GEN_INC_PATH}/tracing.gen)
  add_custom_command(OUTPUT ${L0_GEN_INC_PATH}/tracing.gen
                     COMMAND "${PYTHON_EXECUTABLE}" ${L0_GEN_SCRIPT} ${L0_GEN_INC_PATH} "${L0_INC_PATH}/level_zero")
  target_include_directories(${TARGET}
    PRIVATE "$<BUILD_INTERFACE:${L0_GEN_INC_PATH}>")
  add_dependencies(${TARGET}
    ze_gen_headers)
endmacro()

macro(FindHeadersPath TARGET L0_GEN_SCRIPT GEN_FILE_NAME custom_target L0_TARGET)
  RequirePythonInterp()

  # Use the target that links level zero to find the level zero library
  get_target_property(L0_TARGET_PATH ${L0_TARGET} INTERFACE_INCLUDE_DIRECTORIES)

  # HINTS before PATHS
  find_path(L0_INC_PATH
    NAMES level_zero
    HINTS ${L0_TARGET_PATH}
    PATHS ENV CPATH)
  if (NOT L0_INC_PATH)
    message(FATAL_ERROR
      "Level Zero headers path is not found.\n"
      "You may need to install oneAPI Level Zero Driver to fix this issue.")
  else()
    message(STATUS "Level Zero headers are found at ${L0_INC_PATH}")
  endif()

  set(L0_GEN_INC_PATH "${CMAKE_BINARY_DIR}")
  add_custom_target(${custom_target} ALL
                    DEPENDS ${L0_GEN_INC_PATH}/${GEN_FILE_NAME})
  add_custom_command(OUTPUT ${L0_GEN_INC_PATH}/${GEN_FILE_NAME}
                     COMMAND "${PYTHON_EXECUTABLE}" ${L0_GEN_SCRIPT} ${L0_GEN_INC_PATH} "${L0_INC_PATH}/level_zero")
  target_include_directories(${TARGET}
    PUBLIC "$<BUILD_INTERFACE:${L0_GEN_INC_PATH}>")
  add_dependencies(${TARGET}
    ${custom_target})
endmacro()

macro(CheckForOMPTHeaders)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(omp-tools.h OMPT_INC_FOUND)
  if(NOT OMPT_INC_FOUND)
    message(FATAL_ERROR
      "OMPT is not supported. Try to update your compiler.")
  endif()
endmacro()

macro(FindDRMLibrary TARGET)
  if(DEFINED ENV{LD_LIBRARY_PATH})
    string(COMPARE EQUAL "$ENV{LD_LIBRARY_PATH}" "" RESULT)
      if (NOT RESULT)
        string(REPLACE ":" ";" SEARCH_LIB_PATH $ENV{LD_LIBRARY_PATH})
      endif()
  endif()

  find_library(DRM_LIB_PATH
    NAMES drm
    PATHS ${SEARCH_LIB_PATH})
  if(NOT DRM_LIB_PATH)
    message(FATAL_ERROR
      "DRM library is not found. You may need to install it to fix this issue.")
  else()
    message(STATUS
      "DRM library is found at ${DRM_LIB_PATH}")
  endif()

  target_link_libraries(${TARGET}
    "${DRM_LIB_PATH}")
endmacro()

macro(CheckDRMHeaders TARGET)
  include(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(drm/i915_drm.h I915_DRM_INC_FOUND)
  if (NOT I915_DRM_INC_FOUND)
    message(FATAL_ERROR
      "DRM headers are not found. You may need to install DRM developer package to fix this issue.")
  endif()

  find_path(DRM_INC_PATH
    NAMES drm)
  if(NOT DRM_INC_PATH)
    message(FATAL_ERROR
    "DRM headers path is not found. You may need to install DRM developer package to fix this issue.")
  else()
    message(STATUS
      "DRM headers path is found at ${DRM_INC_PATH}/drm")
  endif()

  set(CMAKE_REQUIRED_INCLUDES " ${DRM_INC_PATH}/drm")
  CHECK_INCLUDE_FILE_CXX(xf86drm.h XF86DRM_INC_FOUND)
  unset(CMAKE_REQUIRED_INCLUDES)
  if(NOT XF86DRM_INC_FOUND)
    message(FATAL_ERROR
      "DRM headers are not found. You may need to install DRM developer package to fix this issue.")
  endif()

  target_include_directories(${TARGET}
    PUBLIC "${DRM_INC_PATH}/drm")
endmacro()

macro(CheckIfSyclIsAvailable)
  include(CheckCXXCompilerFlag)
  include(CheckIncludeFileCXX)
  CHECK_CXX_COMPILER_FLAG("-fsycl" HAVE_SYCL)

  if(HAVE_SYCL)
    CHECK_INCLUDE_FILE_CXX("sycl/sycl.hpp" SYCL_IS_AVAILABLE "-fsycl")
    if(NOT SYCL_IS_AVAILABLE)
      set(HAVE_SYCL NO)
    else()
      message(STATUS "Able to compile sycl code. All samples will be built.")
    endif()
  endif()

  if(NOT HAVE_SYCL)
    message(
      STATUS
        "Not able to compile sycl code. Some tests may not run and some samples may not be built."
    )
  endif()
endmacro()

set(SPDLOG_FMT_URL "https://github.com/fmtlib/fmt/archive/refs/tags/10.1.1.tar.gz")
set(SPDLOG_FMT_SHA256 78B8C0A72B1C35E4443A7E308DF52498252D1CEFC2B08C9A97BC9EE6CFE61F8B)
set(SPDLOG_GABIME_URL "https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz")
set(SPDLOG_GABIME_SHA256 "4DCCF2D10F410C1E2FEAFF89966BFC49A1ABB29EF6F08246335B110E001E09A9")
macro(GetSpdlog)
  if(NOT TARGET spdlog::spdlog)
    find_package(spdlog 1.6.0 QUIET)
  endif()

  if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    if(CMAKE_VERSION VERSION_LESS "3.24")
      FetchContent_Declare(
        fmt
        URL ${SPDLOG_FMT_URL}
        URL_HASH
        SHA256=${SPDLOG_FMT_SHA256}
      )
      FetchContent_Declare(
        spdlog
        URL ${SPDLOG_GABIME_URL}
        URL_HASH
        SHA256=${SPDLOG_GABIME_SHA256}
        )
    else()
      FetchContent_Declare(
        fmt
        URL ${SPDLOG_FMT_URL}
        URL_HASH
        SHA256=${SPDLOG_FMT_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
      )
      FetchContent_Declare(
        spdlog
        URL ${SPDLOG_GABIME_URL}
        URL_HASH
        SHA256=${SPDLOG_GABIME_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
    endif()
    set(FMT_SYSTEM_HEADERS
        ON
        CACHE BOOL "" FORCE)
    set(FMT_INSTALL
        OFF
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED
        OFF
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_PIC
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL_HO
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_SYSTEM_INCLUDES
        ON
        CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_WARNINGS
        OFF
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(fmt spdlog)

    # Prevent fmt from using exceptions because it could throw while logging.
    # Disable warning in fmt due to our usage of EHsc.
    target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
    target_compile_options(fmt PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd6285 $<$<CONFIG:Release>:/wd4702 /wd6385>>)
    target_compile_definitions(fmt-header-only INTERFACE FMT_EXCEPTIONS=0)
    target_compile_options(fmt-header-only INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/wd6285 $<$<CONFIG:Release>:/wd4702 /wd6385>>)

    # spdlog sets the /MP flag on MSVC which causes a warning on icx.
    target_compile_options(spdlog PRIVATE $<$<CXX_COMPILER_ID:IntelLLVM>:-Wno-unused-command-line-argument>)
  endif()
endmacro()

set(GetGTest_URL "https://github.com/google/googletest/archive/refs/tags/v1.15.0.tar.gz")
set(GetGTest_SHA256 7315acb6bf10e99f332c8a43f00d5fbb1ee6ca48c52f6b936991b216c586aaad)
macro(GetGTest)
  if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
    include(FetchContent)
    if(CMAKE_VERSION VERSION_LESS "3.24")
      FetchContent_Declare(
        googletest
        URL ${GetGTest_URL}
        URL_HASH
        SHA256=${GetGTest_SHA256}
        )
    else()
      FetchContent_Declare(
        googletest
        URL ${GetGTest_URL}
        URL_HASH
        SHA256=${GetGTest_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
    endif()
    set(INSTALL_GTEST
        OFF
        CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(PTI_GTEST_COMPILE_OPTIONS
      $<$<CXX_COMPILER_ID:IntelLLVM>:$<$<CONFIG:Release>:-Wno-deprecated-declarations>>
      $<$<CXX_COMPILER_ID:IntelLLVM>:$<$<CONFIG:RelWithDebInfo>:-Wno-deprecated-declarations>>
      $<$<CXX_COMPILER_ID:MSVC>:/wd6239 /wd6031 /wd6387>)

    target_compile_options(gmock_main PRIVATE ${PTI_GTEST_COMPILE_OPTIONS})
    target_compile_options(gmock PRIVATE ${PTI_GTEST_COMPILE_OPTIONS})
    target_compile_options(gtest PRIVATE ${PTI_GTEST_COMPILE_OPTIONS})
    target_compile_options(gtest_main PRIVATE ${PTI_GTEST_COMPILE_OPTIONS})
  endif()
endmacro()

macro(CheckSOVersion PROJ_SOVERSION)
  # Not automatically set because this should be done intentionally.
  # PTI's rules for backwords compatibility should be re-evaluated upon first
  # major / production release.
  # PTI is following [semver](https://semver.org/) versioning and we are using
  # the SOVERSION to denote backword compatibility.
  if ("${PROJECT_VERSION}" VERSION_GREATER_EQUAL "1.0.0")
    if("${PROJ_SOVERSION}" STREQUAL "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
      message(AUTHOR_WARNING "${PROJECT_NAME} currently has the SOVERSION: "
                    "${PROJ_SOVERSION}. After the first major release, ${PROJECT_NAME}"
                    " should have the SOVERSION: ${PROJECT_VERSION_MAJOR}.")
    endif()
  else()
    if("${PROJ_SOVERSION}" STREQUAL "${PROJECT_VERSION_MAJOR}")
      message(AUTHOR_WARNING "${PROJECT_NAME} currently has the SOVERSION: "
                    "${PROJ_SOVERSION}. During development, ${PROJECT_NAME}"
                    " should have the SOVERSION:"
                    " ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.")
    endif()
  endif()
endmacro()

macro(GetLevelZero)
  if (NOT TARGET LevelZero::level-zero)
    # Need zelEnableTracingLayer
    set(LZ_VER_MAJOR "1")
    set(LZ_VER_MINOR "17")
    set(LZ_VER_PATCH "25")
    set(LZ_VER "${LZ_VER_MAJOR}.${LZ_VER_MINOR}.${LZ_VER_PATCH}")

    include(FetchContent)
    FetchContent_Declare(
        LevelZero
        URL
        https://github.com/oneapi-src/level-zero/archive/refs/tags/v${LZ_VER}.tar.gz
        URL_HASH
        SHA256=3cfa1eb001d5974efed3002b6a5e6e687c7413141b3ae26e8bdac8085acddb9e
    )
    # Prevent content from automatically being installed with PTI
    FetchContent_GetProperties(LevelZero)
    if(NOT LevelZero_POPULATED)
        FetchContent_Populate(LevelZero)
        # Add patch to L0 build
        file(WRITE "${levelzero_SOURCE_DIR}/VERSION_PATCH" ${LZ_VER_PATCH})
        add_subdirectory(${levelzero_SOURCE_DIR} ${levelzero_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    # Create new target to treat level zero loader as an external dependency.
    # This prevents it from being added to the export set.
    # (Basically treat as if including via find_package)
    add_library(pti_ze_loader INTERFACE IMPORTED)
    add_dependencies(pti_ze_loader ze_tracing_layer)

    set(PTI_LZ_COMPILE_OPTIONS $<$<CXX_COMPILER_ID:IntelLLVM>:-Wno-unused-parameter>
                               $<$<CXX_COMPILER_ID:MSVC>:/wd6285
                                $<$<CONFIG:Release>:/wd4702 /wd6385 /wd6386>>)

    # Silence Warnings from Level Zero Loader. Allows us to better detect PTI
    # warnings and errors.
    target_compile_options(ze_loader PRIVATE ${PTI_LZ_COMPILE_OPTIONS})
    target_compile_options(ze_tracing_layer PRIVATE ${PTI_LZ_COMPILE_OPTIONS})
    target_compile_options(ze_null PRIVATE ${PTI_LZ_COMPILE_OPTIONS})
    target_compile_options(ze_validation_layer PRIVATE ${PTI_LZ_COMPILE_OPTIONS})
    target_compile_options(utils PRIVATE ${PTI_LZ_COMPILE_OPTIONS})

    # Pull Headers out of source tree and add them to level_zero/
    # This allows us to keep the normal way to include level zero
    # TODO(matthew.schilling@intel.com): Should we move PTI headers to
    # <proj_dir>/include/pti/?
    file(GLOB_RECURSE L0_DL_HEADERS
        LIST_DIRECTORIES TRUE
        "${CMAKE_CURRENT_BINARY_DIR}/_deps/levelzero-src/include/*")

    file(COPY ${L0_DL_HEADERS}
        DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}/_deps/levelzero-headers/include/level_zero/)

    # Add new header path to our new target
    find_path(LZ_INCLUDE_DIR
      NAMES level_zero/ze_api.h
      HINTS ${CMAKE_CURRENT_BINARY_DIR}/_deps/levelzero-headers
      PATH_SUFFIXES include
      NO_PACKAGE_ROOT_PATH
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )

    set_target_properties(pti_ze_loader PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES ${LZ_INCLUDE_DIR})

    target_link_libraries(pti_ze_loader INTERFACE
                            $<BUILD_INTERFACE:ze_loader>)
    message(WARNING "Linking different version of Level Zero Loader, please set"
                    " $LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/path/to/l0/:$LD_LIBRARY_PATH "
                    "Or Install Level Zero Loader Version >= ${LZ_VER} on your"
                    "system")
    # Add Alias target to treat it as if we found it via find_packge
    add_library(LevelZero::level-zero ALIAS pti_ze_loader)
  endif()
endmacro()

macro(RemoveNDebugFlag)
  if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM" OR CMAKE_CXX_COMPILER_ID MATCHES
                                                  "Clang")
    # One must make sure -DNDEBUG is not set if using XPTI filename/linenumber
    # https://stackoverflow.com/questions/22140520/how-to-enable-assert-in-cmake-release-mode
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE
                   "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO
                   "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endif()
endmacro()

macro(ProjectIsTopLevel)
  if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    set(PROJECT_IS_TOP_LEVEL FALSE)
    get_property(PTI_PROJ_PARENT DIRECTORY PROPERTY PARENT_DIRECTORY)
    if(PTI_PROJ_PARENT STREQUAL "")
      set(PROJECT_IS_TOP_LEVEL TRUE)
    endif()
  endif()
endmacro()

macro(AddFormatTarget)
  find_program(CLANG_FORMAT_EXE clang-format)
  find_program(BLACK_FORMAT_EXE black)

  add_custom_target(format)
  add_custom_target(format-chk)

  cmake_policy(PUSH)
  cmake_policy(SET CMP0009 NEW)
  file(GLOB_RECURSE cf_src_files  "${PROJECT_SOURCE_DIR}/src/*.cc"
                                  "${PROJECT_SOURCE_DIR}/src/*.h"
                                  "${PROJECT_SOURCE_DIR}/src/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/src/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/test/*.cc"
                                  "${PROJECT_SOURCE_DIR}/test/*.h"
                                  "${PROJECT_SOURCE_DIR}/test/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/test/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/samples/*.cc"
                                  "${PROJECT_SOURCE_DIR}/samples/*.h"
                                  "${PROJECT_SOURCE_DIR}/samples/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/samples/**/*.cc"
                                  "${PROJECT_SOURCE_DIR}/fuzz/*.cc"
                                  "${PROJECT_SOURCE_DIR}/fuzz/*.h"
                                  "${PROJECT_SOURCE_DIR}/fuzz/**/*.h"
                                  "${PROJECT_SOURCE_DIR}/fuzz/**/*.cc")
  list(FILTER cf_src_files EXCLUDE REGEX
                                  "^${PROJECT_SOURCE_DIR}/samples/dlworkload*")
  list(FILTER cf_src_files EXCLUDE REGEX
                                   "^${PROJECT_SOURCE_DIR}/samples/iso3d*")
  list(FILTER cf_src_files EXCLUDE REGEX
                                  "^${PROJECT_SOURCE_DIR}/build*")

  add_custom_target(format-cpp COMMAND ${CLANG_FORMAT_EXE} -i ${cf_src_files})
  add_custom_target(format-cpp-chk COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${cf_src_files})

  add_dependencies(format format-cpp)
  add_dependencies(format-chk format-cpp-chk)

  if(BLACK_FORMAT_EXE)
    list(APPEND py_src_dirs "${PROJECT_SOURCE_DIR}/cmake"
                            "${PROJECT_SOURCE_DIR}/src"
                            "${PROJECT_SOURCE_DIR}/fuzz"
                            "${PROJECT_SOURCE_DIR}/test"
                            "${PROJECT_SOURCE_DIR}/samples")
    file(GLOB py_src_dirs_cur "${PROJECT_SOURCE_DIR}/*.py")
    set(py_src_dirs ${py_src_dirs} ${py_src_dirs_cur})
    add_custom_target(format-py COMMAND ${BLACK_FORMAT_EXE} ${py_src_dirs})
    add_custom_target(format-py-chk COMMAND ${BLACK_FORMAT_EXE} --check ${py_src_dirs})

    add_dependencies(format format-py)
    add_dependencies(format-chk format-py-chk)
  else()
    message(STATUS "black not found. Python code cannot be formatted.")
  endif()
  cmake_policy(POP)
endmacro()

macro(AddProjectVersionInfo TARGET)
  if (WIN32)
    set(PTI_VERSIONINFO_RC "${PROJECT_BINARY_DIR}/${TARGET}_versioninfo.rc")
    set(PTI_VERSIONINFO_RC_IN "${PROJECT_SOURCE_DIR}/cmake/Modules/pti_versioninfo.rc.in")

    set(INTEL_LIC_PATH "${PTI_CMAKE_MACRO_DIR}/../LICENSE")

    if (EXISTS "${INTEL_LIC_PATH}")
      file(STRINGS "${INTEL_LIC_PATH}" INTEL_LICENSE_FILE_STRINGS)
      foreach(PTI_STRING ${INTEL_LICENSE_FILE_STRINGS})
        string(REGEX MATCH "^Copyright.*" PTI_COPYRIGHT "${PTI_STRING}")
        if (PTI_COPYRIGHT)
          break()
        endif()
      endforeach()
    endif()

    if(NOT PTI_COPYRIGHT)
      message(WARNING "Copyright file not found, Windows versioninfo will be incomplete")
      set(PTI_COPYRIGHT "")
    endif()

    configure_file(${PTI_VERSIONINFO_RC_IN} ${PTI_VERSIONINFO_RC} @ONLY)
    target_sources(${TARGET} PRIVATE ${PTI_VERSIONINFO_RC})
  endif()
endmacro()
