if(WIN32)
  if(CMAKE_VERSION VERSION_LESS "3.20")
    message(WARNING "sample requires CMake version 3.20 or greater.")
    return()
  endif()
  cmake_minimum_required(VERSION 3.20)
  find_package(oneDPLWindowsIntelLLVM)
else()
  cmake_minimum_required(VERSION 3.14)
endif()


project(dlworkload_demo_with_syclgraph_capture)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

option(USE_HOST_MEMORY "Use USE_HOST_MEMORY" OFF)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" has_sycl)

if(NOT has_sycl)
  message(WARNING "${PROJECT_NAME} requires a sycl compatible compiler.")
  return()
endif()

if (NOT TARGET oneDPL)
  find_package(oneDPL)
endif()

if (NOT TARGET oneDPL)
  message(WARNING "${PROJECT_NAME} requires a oneDPL installation.")
  return ()
endif()

if (NOT TARGET TBB::tbb)
  find_package(TBB)
endif()

if (NOT TARGET TBB::tbb)
  message(WARNING "${PROJECT_NAME} requires a Intel TBB installation.")
  return ()
endif()

if (NOT TARGET DNNL::dnnl)
  find_package(dnnl)
endif()

if (NOT TARGET DNNL::dnnl)
  message(WARNING "${PROJECT_NAME} requires a DNNL installation.")
  return ()
endif()

if (NOT TARGET MKL::MKL)
  find_package(MKL CONFIG PATHS $ENV{MKLROOT} NO_DEFAULT_PATH)
  message(STATUS "Imported oneMKL targets: ${MKL_IMPORTED_TARGETS}")
endif()

if (NOT TARGET MKL::MKL)
  message(WARNING "${PROJECT_NAME} requires a oneMKL installation.")
  return ()
endif()

FILE(GLOB SOURCES "*.cpp")
add_executable(dlworkload ${SOURCES})

target_compile_options(dlworkload PUBLIC -fsycl -gline-tables-only)

target_link_options(dlworkload PUBLIC -fsycl -gline-tables-only)

if(USE_HOST_MEMORY)
  target_compile_definitions(dlworkload PUBLIC USE_HOST_MEMORY)
endif()

target_include_directories(dlworkload PRIVATE ${PROJECT_SOURCE_DIR}/../samples_utilities)

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

target_include_directories(dlworkload SYSTEM PUBLIC $<TARGET_PROPERTY:MKL::MKL,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(dlworkload DNNL::dnnl MKL::MKL MKL::MKL_SYCL oneDPL Pti::pti_view)
