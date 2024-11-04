set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Modules/")
include(macros)

setrequiredcmakeversion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project (METRICSISO3DFD)

setcompilerflags()
setbuildtype()

if (NOT CMAKE_BUILD_TYPE)
    message (STATUS "Default CMAKE_BUILD_TYPE not set using Release with Debug Info")
    set (CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE
        STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel"
        FORCE)
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler.")
  return()
endif()

if (NOT TARGET DevUtilities::utils)
  find_package(DevUtilities)
endif()

if (NOT TARGET DevUtilities::utils)
  message(WARNING "${PROJECT_NAME} requires dev-utilities installed.")
  return()
endif()

if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

if (NOT TARGET Pti::pti_metrics)
  find_package(Pti REQUIRED)
endif()

if (NOT TARGET LevelZero::level-zero)
  find_package(LevelZero REQUIRED)
endif()

add_subdirectory (src)
