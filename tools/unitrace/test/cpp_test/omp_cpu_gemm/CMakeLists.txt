if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qopenmp")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -qopenmp")
endif()

project(omp_cpu_gemm CXX)

add_executable(omp_cpu_gemm main.cc)

add_test(NAME omp_cpu_gemm
         COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../run_test.py
            -c ${CMAKE_SOURCE_DIR} omp_cpu_gemm)
