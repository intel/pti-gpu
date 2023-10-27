option(SHARED_KERNEL "Use SLM Kernel Version - Only for GPU" OFF)

# Set default build type to RelWithDebInfo if not specified
if(NOT CMAKE_BUILD_TYPE)
  message(
    STATUS "Default CMAKE_BUILD_TYPE not set using Release with Debug Info")
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE
        STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel"
        FORCE)
endif()

add_executable(iso3dfd.exe iso3dfd.cpp iso3dfd_kernels.cpp utils.cpp)
target_compile_options(iso3dfd.exe PUBLIC -O3 -fsycl --std=c++17)
target_link_options(iso3dfd.exe PUBLIC -O3 -fsycl --std=c++17)
target_include_directories(
  iso3dfd.exe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../samples_utilities
                      ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(iso3dfd.exe PUBLIC Pti::pti_view sycl)

if(SHARED_KERNEL)
  target_compile_definitions(iso3dfd.exe PUBLIC USED_SHARED)
endif()

if(WIN32)
  add_custom_target(runiso iso3dfd.exe 256 256 256 32 8 64 10 gpu)
  add_custom_target(runiso_cpu iso3dfd.exe 256 256 256 256 1 1 10 cpu)
else()
  add_custom_target(runiso iso3dfd.exe 256 256 256 32 8 64 10 gpu)
  add_custom_target(runiso_cpu iso3dfd.exe 256 256 256 256 1 1 10 cpu)
endif()

# set(CMAKE_EXE_LINKER_FLAGS " ${CMAKE_EXE_LINKER_FLAGS}")
# set(CMAKE_EXE_LINKER_FLAGS " -fsycl -lmkl_sycl -lmkl_intel_ilp64
# -lmkl_sequential -lmkl_core")
