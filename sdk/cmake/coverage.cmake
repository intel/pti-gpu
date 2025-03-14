RequirePythonInterp()

if(DEFINED PTI_INSTALL)
  set(PTI_INSTALL_COVERAGE
      ${PTI_INSTALL}
      CACHE
      BOOL
      "Option to install coverage artifacts.")
else()
  set(PTI_INSTALL_COVERAGE
      ON
      CACHE
      BOOL
      "Option to install coverage artifacts.")
endif()

set(DEFAULT_PROFDATA_MERGE_FILENAME
	"pti-cov.profdata"
	CACHE
	STRING
	"Default filename of merged data (i.e., output from llvm-profdata)")

#
# Source coverage file structure
#
# <src>
# <src>/<bin>/coverage
# <src>/<bin>/coverage/raw (raw data generated from tests)
# <src>/<bin>/coverage/html (html report)
# <src>/<bin>/coverage/*.info (exported lcov for 3rd party integration)
# <src>/<bin>/coverage/*.profdata (merged raw data use to generate output)
#

if(NOT PTI_COVERAGE_DIR)
  set(PTI_COVERAGE_DIR ${PROJECT_BINARY_DIR}/coverage)
  set(PTI_COVERAGE_RAW_DIR ${PTI_COVERAGE_DIR}/raw)
endif()

if(NOT PTI_PROFDATA_MERGE_FILENAME)
  set(PTI_PROFDATA_MERGE_FILENAME ${DEFAULT_PROFDATA_MERGE_FILENAME})
endif()

set(PTI_PROFDATA_MERGE_OUTPUT
    ${PTI_COVERAGE_DIR}/${PTI_PROFDATA_MERGE_FILENAME})

set(THIRD_PARTY_SRC_DIR ".*_deps*")

if(FETCHCONTENT_BASE_DIR)
  set(THIRD_PARTY_SRC_DIR "${THIRD_PARTY_SRC_DIR}|${FETCHCONTENT_BASE_DIR}/*")
  file(TO_NATIVE_PATH "${THIRD_PARTY_SRC_DIR}" THIRD_PARTY_SRC_DIR)
endif()

# CMPLR_ROOT -> prefer Intel oneAPI compiler bundled llvm
if(NOT LLVM_PROFDATA_PROG)
  find_program(
    LLVM_PROFDATA_PROG llvm-profdata
    HINTS ENV CMPLR_ROOT
    PATH_SUFFIXES bin bin/compiler)
endif()

if(NOT LLVM_COV_PROG)
  find_program(
    LLVM_COV_PROG llvm-cov
    HINTS ENV CMPLR_ROOT
    PATH_SUFFIXES bin bin/compiler)
endif()

if(NOT LLVM_PROFDATA_PROG)
  return()
endif()

if(NOT LLVM_COV_PROG)
  return()
endif()

add_custom_command(
  OUTPUT ${PTI_PROFDATA_MERGE_OUTPUT}
  COMMAND ${LLVM_PROFDATA_PROG} merge -output=${PTI_PROFDATA_MERGE_OUTPUT}
          ${PTI_COVERAGE_RAW_DIR}/*.profraw
  DEPENDS ${PTI_COVERAGE_RAW_DIR})

set(CODE_COVERAGE_BASE_CMD
    ${Python_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/cmake/generate_coverage_report.py
    --llvm-cov
    ${LLVM_COV_PROG}
    --objects
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    --data
    ${PTI_PROFDATA_MERGE_OUTPUT}
    --sources
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
    --extra=-ignore-filename-regex=${THIRD_PARTY_SRC_DIR})

add_custom_target(
  coverage_summary
  COMMAND ${CODE_COVERAGE_BASE_CMD} --summary
  DEPENDS ${PTI_PROFDATA_MERGE_OUTPUT}
  COMMENT "Generate coverage summary"
  VERBATIM)

add_custom_target(
  coverage_report
  COMMAND ${CODE_COVERAGE_BASE_CMD} --lcov ${PTI_COVERAGE_DIR}/coverage.info
  COMMAND ${CODE_COVERAGE_BASE_CMD} --output-dir ${PTI_COVERAGE_DIR}/html
  DEPENDS ${PTI_PROFDATA_MERGE_OUTPUT}
  COMMENT "Generate html coverage report along with lcov export data."
  VERBATIM)

if(PTI_INSTALL_COVERAGE)
  install(
    DIRECTORY "${PTI_COVERAGE_DIR}/"
    CONFIGURATIONS Coverage
    DESTINATION coverage
    COMPONENT Pti_Coverage_Data)

  install(
    DIRECTORY "${PTI_COVERAGE_DIR}/html"
    CONFIGURATIONS Coverage
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/coverage
    COMPONENT Pti_Coverage_HTML)
endif()
