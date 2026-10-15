
cmake_minimum_required(VERSION 3.22)

if(WIN32)
  set(CMAKE_C_COMPILER "$ENV{CMPLR_ROOT}/bin/icx.exe")
  set(CMAKE_CXX_COMPILER "$ENV{CMPLR_ROOT}/bin/icx.exe")
else()
  set(CMAKE_C_COMPILER "$ENV{CMPLR_ROOT}/bin/icx")
  set(CMAKE_CXX_COMPILER "$ENV{CMPLR_ROOT}/bin/icpx")
endif()

project(UnitraceTest C CXX)

# Unitrace's own cmake modules, included once so every workload can use them:
# the pinned Level Zero headers (HeaderFetch) and, on Windows, the loader
# import library (LevelZero).
include("${PROJECT_SOURCE_DIR}/../../cmake/HeaderFetch.cmake")
if(WIN32)
  include("${PROJECT_SOURCE_DIR}/../../cmake/LevelZero.cmake")
endif()

find_package(Python 3.9 COMPONENTS Interpreter REQUIRED)

# Some compiler distributions ship an IntelSYCLConfig.cmake whose SYCL
# feature test fails even though icpx -fsycl works. With this option ON,
# add_sycl_to_target() applies plain -fsycl instead of using the package.
option(UNITRACE_PLAIN_SYCL
  "Use plain -fsycl flags instead of the IntelSYCL cmake package" OFF)
if (UNITRACE_PLAIN_SYCL)
  function(add_sycl_to_target target)
    target_compile_options(${target} PRIVATE -fsycl)
    target_link_options(${target} PRIVATE -fsycl)
  endfunction()
else()
  find_package(IntelSYCL REQUIRED)
endif()

# L0 version required
set (LEVEL_ZERO_COMMIT_ID "v1.32.0")

# Workloads are built here and installed (test_unitrace.py runs
# cmake --install --prefix <unitrace build>/test) into bin/<workload>/, one
# directory per workload, together with its data files (.spv kernels are loaded
# from next to the executable). Install a new workload the same way:
#   install(TARGETS <name> RUNTIME DESTINATION bin/<name> COMPONENT <name>)
# Keep the link-time library paths so the installed binaries run like the
# built ones.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")


add_subdirectory(cl_gemm)
add_subdirectory(conditional_trace)
add_subdirectory(dpc_gemm)
add_subdirectory(device_and_rank_to_sample)
add_subdirectory(event_reset)
add_subdirectory(gemm_stall_sampling)
add_subdirectory(graph)
add_subdirectory(grf)
# internal_conditional_trace exercises the in-application stop control
# (__itt_detach / PTI_ENABLE_COLLECTION).
add_subdirectory(internal_conditional_trace)
add_subdirectory(omp_gemm)
add_subdirectory(omp_cpu_gemm)
add_subdirectory(sycl_graph_with_v2_adaptor)
add_subdirectory(temporal)
add_subdirectory(ze_gemm)
add_subdirectory(ze_graph_api)
add_subdirectory(ze_counterbased_events)
