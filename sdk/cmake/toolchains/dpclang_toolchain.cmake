set(CMAKE_C_COMPILER "dpclang")
set(CMAKE_CXX_COMPILER "dpclang++")

find_program(DPCLANGXX_PATH NAMES dpclang++)
get_filename_component(_bin_dir ${DPCLANGXX_PATH} DIRECTORY)
get_filename_component(_prefix  "${_bin_dir}" DIRECTORY)
list(PREPEND CMAKE_PREFIX_PATH "${_prefix}")
