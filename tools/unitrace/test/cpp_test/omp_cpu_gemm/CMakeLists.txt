if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qopenmp")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -qopenmp")
endif()

project(omp_cpu_gemm CXX)

add_executable(omp_cpu_gemm main.cc)

install(TARGETS omp_cpu_gemm RUNTIME DESTINATION bin/omp_cpu_gemm COMPONENT omp_cpu_gemm)
