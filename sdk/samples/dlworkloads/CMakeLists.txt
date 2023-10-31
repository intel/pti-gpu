cmake_minimum_required(VERSION 3.5)
project(dlworkload_demo_with_syclgraph_capture)

option(USE_HOST_MEMORY "Use USE_HOST_MEMORY" OFF)
if(USE_HOST_MEMORY)
    add_definitions(-DUSE_HOST_MEMORY)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

add_definitions(-Wall)
#set (CMAKE_CXX_COMPILER "dpcpp")
FILE(GLOB SOURCES "*.cpp")
add_compile_options(-fsycl -gline-tables-only)
add_link_options(-fsycl -gline-tables-only)

add_executable(dlworkload ${SOURCES})
target_include_directories(dlworkload PRIVATE ../samples_utilities)

find_package(oneDPL REQUIRED)

# Allow building in-source and out-of-source
if (NOT TARGET Pti::pti_view)
  find_package(Pti REQUIRED)
endif()

if (NOT TARGET DNNL::dnnl)
  find_package(dnnl REQUIRED)
endif()

target_include_directories(dlworkload SYSTEM PUBLIC $ENV{MKLROOT}/include)

target_link_libraries(dlworkload PUBLIC DNNL::dnnl -lmkl_sycl -lmkl_intel_ilp64 -lmkl_core -lmkl_tbb_thread oneDPL Pti::pti_view)

