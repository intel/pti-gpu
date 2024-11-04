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

add_executable(metrics_iso3dfd iso3dfd.cpp iso3dfd_kernels.cpp utils.cpp)
target_compile_options(metrics_iso3dfd PUBLIC -O3 -fsycl)
target_link_options(metrics_iso3dfd PUBLIC -O3 -fsycl)
target_include_directories(
  metrics_iso3dfd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../samples_utilities
                      ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(metrics_iso3dfd PUBLIC spdlog::spdlog Pti::pti_view Pti::pti_metrics LevelZero::level-zero DevUtilities::utils)

if(SHARED_KERNEL)
  target_compile_definitions(metrics_iso3dfd PUBLIC USED_SHARED)
endif()

if(WIN32)
  add_custom_target(runmetricsiso metrics_iso3dfd 256 256 256 32 8 64 10 gpu)
  add_custom_target(runmetricsiso_cpu metrics_iso3dfd 256 256 256 256 1 1 10 cpu)
else()
  add_custom_target(runmetricsiso metrics_iso3dfd 256 256 256 32 8 64 10 gpu)
  add_custom_target(runmetricsiso_cpu metrics_iso3dfd 256 256 256 256 1 1 10 cpu)
endif()
