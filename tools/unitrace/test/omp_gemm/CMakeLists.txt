if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qiopenmp -Qopenmp-targets=spir64")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /Qopenmp /Qopenmp-targets=spir64")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fiopenmp -fopenmp-targets=spir64")
endif()

project(omp_gemm CXX) 

add_executable(omp_gemm main.cc)

if(WIN32)
  target_link_libraries(omp_gemm PRIVATE "$ENV{ONEAPI_ROOT}/compiler/latest/lib/libiomp5md.lib")
endif()

add_test(NAME omp_gemm COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/run_test.py ${CMAKE_SOURCE_DIR} omp_gemm GPU)