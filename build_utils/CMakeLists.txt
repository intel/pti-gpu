set(PTI_CMAKE_MACRO_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

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
  if(WIN32)
    set(ADD_FLAGS " ")
    add_definitions(-DNOMINMAX)
  else()
    set(ADD_FLAGS " -fvisibility=hidden")
  endif()
  file(STRINGS "${PTI_CMAKE_MACRO_DIR}/../VERSION" PTI_VERSION)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADD_FLAGS} -DPTI_VERSION=${PTI_VERSION}")
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
    PRIVATE -DCL_TARGET_OPENCL_VERSION=300)
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
      NAMES igdmd md
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

macro(GetGTPinPackage)
  if(NOT DEFINED GTPIN_PATH)
    if(NOT DEFINED GTPIN_LINK)
      if(UNIX)
        set(GTPIN_LINK https://downloadmirror.intel.com/867025/external-release-gtpin-4.6.4-linux.tar.xz)
      elseif(WIN32)
        set(GTPIN_LINK https://downloadmirror.intel.com/867025/external-release-gtpin-4.6.4-win.zip)
      else()
        message(FATAL_ERROR "Graphics Technology Pin (GT Pin) is not supported for the platform")
      endif()
    endif()
    include(FetchContent)
    if(${CMAKE_VERSION} VERSION_LESS "3.24.0")
      FetchContent_Declare(
        gtpin_package
        URL ${GTPIN_LINK}
SOURCE_DIR ${GTPIN_SOURCE_DIR}
      )
    else()
      FetchContent_Declare(
        gtpin_package
        URL ${GTPIN_LINK}
        DOWNLOAD_EXTRACT_TIMESTAMP YES
SOURCE_DIR ${GTPIN_SOURCE_DIR}
      )
    endif()
    message(STATUS "Download GTPin package link: ${GTPIN_LINK}")
    FetchContent_MakeAvailable(gtpin_package)
    if(UNIX)
      set(GTPIN_PATH ${gtpin_package_SOURCE_DIR})
    elseif(WIN32)
      set(GTPIN_PATH ${gtpin_package_SOURCE_DIR}/Profilers)
    else()
      message(FATAL_ERROR "Graphics Technology Pin (GT Pin) is not supported for the platform")
    endif()
    message(STATUS "GTPin Technology Pin (GT Pin) unpacked: ${GTPIN_PATH}")
  else()
    if(NOT IS_ABSOLUTE ${GTPIN_PATH})
      message(FATAL_ERROR "Provided GTPIN_PATH is not an absolute path: ${GTPIN_PATH}")
    endif()
  endif()
  find_library(GTPIN_LIB_PATH
    NAMES gtpin
    HINTS ${GTPIN_PATH}/Lib/intel64)
  find_library(GTPIN_GED_LIB_PATH
    NAMES ged
    HINTS ${GTPIN_PATH}/Lib/intel64)
  find_path(GTPIN_FILE_GTPIN_API_H "gtpin_api.h"
    PATHS ${GTPIN_PATH}/Include/api/)
  find_path(GTPIN_FILE_GED_H "ged.h"
    PATHS ${GTPIN_PATH}/Include/ged/intel64)
  if(NOT (GTPIN_LIB_PATH AND GTPIN_GED_LIB_PATH AND GTPIN_FILE_GTPIN_API_H AND GTPIN_FILE_GED_H))
    message(FATAL_ERROR "Some parts of GTPin package not found: GTPIN_LIB_PATH:${GTPIN_LIB_PATH} ; GTPIN_GED_LIB_PATH:${GTPIN_GED_LIB_PATH} ; GTPIN_FILE_GTPIN_API_H:${GTPIN_FILE_GTPIN_API_H} ; GTPIN_FILE_GED_H:${GTPIN_FILE_GED_H}")
  endif()
endmacro()

macro(FindGTPinLibrary TARGET)
  GetGTPinPackage()
  target_link_directories(${TARGET}
    PRIVATE ${GTPIN_PATH}/Lib/intel64)
  target_link_libraries(${TARGET} gtpin ged)
endmacro()

macro(FindGTPinHeaders TARGET)
  GetGTPinPackage()
  target_include_directories(${TARGET}
    PRIVATE ${GTPIN_PATH}/Include
    PRIVATE ${GTPIN_PATH}/Include/api
    PRIVATE ${GTPIN_PATH}/Include/ged/intel64)
endmacro()

macro(FindGTPinUtils TARGET)
  GetGTPinPackage()
  if(NOT TARGET gtpintool_utils)
    set(GTPIN_KIT ${GTPIN_PATH})
    add_subdirectory(${GTPIN_PATH}/Examples ${CMAKE_CURRENT_BINARY_DIR}/gtpin_utils EXCLUDE_FROM_ALL)
  endif()
  target_include_directories(${TARGET} PRIVATE ${GTPIN_PATH}/Examples/utils)
  target_link_libraries(${TARGET} gtpintool_utils)
endmacro()

macro(GetLevelZeroHeaders TARGET)
  set(L0_INC_PATH "${CMAKE_BINARY_DIR}")
  RequirePythonInterp()

  if (NOT TARGET level_zero_headers)
    add_custom_target(level_zero_headers ALL
                      DEPENDS ${L0_INC_PATH}/level_zero/ze_api.h
                              ${L0_INC_PATH}/level_zero/zes_api.h
                              ${L0_INC_PATH}/level_zero/zet_api.h
                              ${L0_INC_PATH}/level_zero/layers/zel_tracing_api.h
                              ${L0_INC_PATH}/level_zero/layers/zel_tracing_register_cb.h)
    add_custom_command(OUTPUT ${L0_INC_PATH}/level_zero/ze_api.h
                              ${L0_INC_PATH}/level_zero/zes_api.h
                              ${L0_INC_PATH}/level_zero/zet_api.h
                              ${L0_INC_PATH}/level_zero/layers/zel_tracing_api.h
                              ${L0_INC_PATH}/level_zero/layers/zel_tracing_register_cb.h
                      COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/get_ze_headers.py" ${L0_INC_PATH} ${CMAKE_BINARY_DIR} ${LEVEL_ZERO_COMMIT_ID})
  endif()

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
      # cmake uses '\' as escape sequence hence paths ending with '\' gets merged to next path
      #  which gives incorrect result. Creating a copy into a new variable eliminates the issue.
      set(PATH_LIST $ENV{PATH})
      foreach(VALUE ${PATH_LIST})
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
          if (NOT TARGET level_zero_lib)
            RequirePythonInterp()

            add_custom_target(level_zero_lib ALL
                              DEPENDS ${CMAKE_BINARY_DIR}/ze_loader.lib)
            add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ze_loader.lib
                               COMMAND "${PYTHON_EXECUTABLE}" "${PTI_CMAKE_MACRO_DIR}/convert_dll_to_lib.py" ${CMAKE_BINARY_DIR} ${L0_DLL_PATH})
            add_dependencies(${TARGET} level_zero_lib)
            target_link_libraries(${TARGET} "${CMAKE_BINARY_DIR}/ze_loader.lib")
          else()
            add_dependencies(${TARGET} level_zero_lib)
            target_link_libraries(${TARGET} "${CMAKE_BINARY_DIR}/ze_loader.lib")
          endif()
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

    target_link_libraries(${TARGET}
      "${L0_LIB_PATH}")
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
  check_include_file_cxx(level_zero/layers/zel_tracing_register_cb.h LO_LAYERS_TRC_REG_INC_FOUND)
  if ((NOT LO_CORE_INC_FOUND) OR (NOT LO_TOOLS_INC_FOUND) OR (NOT LO_LAYERS_INC_FOUND) OR (NOT LO_SYSMAN_INC_FOUND) OR (NOT LO_LAYERS_TRC_REG_INC_FOUND))
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
    PUBLIC "${L0_GEN_INC_PATH}")
  add_dependencies(${TARGET}
    ze_gen_headers)
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
