
cmake_minimum_required(VERSION 3.22) 

project(UnitracePythonTest)

find_package(PythonInterp 3.9 REQUIRED)

# Enable testing
enable_testing()

add_subdirectory(vec_add)
add_subdirectory(mat_mul)

