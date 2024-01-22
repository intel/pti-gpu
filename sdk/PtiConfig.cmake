# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
include("${CMAKE_CURRENT_LIST_DIR}/PtiTargets.cmake")

include(CMakeFindDependencyMacro)
find_dependency(Threads)
