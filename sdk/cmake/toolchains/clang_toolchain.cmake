set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

find_program(CLANGXX_PATH NAMES clang++)
get_filename_component(_bin_dir ${CLANGXX_PATH} DIRECTORY)
get_filename_component(_prefix  "${_bin_dir}" DIRECTORY)
list(PREPEND CMAKE_PREFIX_PATH "${_prefix}")
