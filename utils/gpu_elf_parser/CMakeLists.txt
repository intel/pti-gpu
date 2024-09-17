set(PTI_DEBUG_INFO_PARSER_BASE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

include("${CMAKE_CURRENT_LIST_DIR}/../../build_utils/CMakeLists.txt")
SetRequiredCMakeVersion()
cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})

project(PTI_debug_info_parser CXX)
SetCompilerFlags()
SetBuildType()

macro(FindPtiElfParserHeaders TARGET)
  target_include_directories(${TARGET}
    PUBLIC "${PTI_DEBUG_INFO_PARSER_BASE_DIR}/include"
    PUBLIC "${PTI_CMAKE_MACRO_DIR}/../utils") # leb128.hpp, pti_assert.h
endmacro()

add_library(debug_info_parser STATIC
    ${PTI_DEBUG_INFO_PARSER_BASE_DIR}/src/elf_parser.cpp
    ${PTI_DEBUG_INFO_PARSER_BASE_DIR}/src/section_debug_line.cpp
    ${PTI_DEBUG_INFO_PARSER_BASE_DIR}/src/section_debug_info.cpp
    ${PTI_DEBUG_INFO_PARSER_BASE_DIR}/src/section_debug_abbrev.cpp
    ${PTI_DEBUG_INFO_PARSER_BASE_DIR}/src/dwarf_state_machine.cpp
)

FindPtiElfParserHeaders(debug_info_parser)
set_property(TARGET debug_info_parser PROPERTY POSITION_INDEPENDENT_CODE ON)

if(CMAKE_INCLUDE_PATH)
  target_include_directories(debug_info_parser
    PUBLIC "${CMAKE_INCLUDE_PATH}")
endif()
