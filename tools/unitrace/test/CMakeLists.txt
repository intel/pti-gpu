
cmake_minimum_required(VERSION 3.22) 

if(WIN32)
  set(CMAKE_C_COMPILER "$ENV{CMPLR_ROOT}/bin/icx.exe")
  set(CMAKE_CXX_COMPILER "$ENV{CMPLR_ROOT}/bin/icx.exe")
else()
  set(CMAKE_C_COMPILER "$ENV{CMPLR_ROOT}/bin/icx")
  set(CMAKE_CXX_COMPILER "$ENV{CMPLR_ROOT}/bin/icpx")
endif()

project(UnitraceTest C CXX)

find_package(PythonInterp 3.9 REQUIRED)
find_package(IntelSYCL REQUIRED)

# Enable testing
enable_testing()

add_subdirectory(graph)
add_subdirectory(cl_gemm)
add_subdirectory(ze_gemm)
add_subdirectory(dpc_gemm)
add_subdirectory(grf)
add_subdirectory(omp_gemm)
add_subdirectory(ze_counterbased_events)
add_subdirectory(temporal)
