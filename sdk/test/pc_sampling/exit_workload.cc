//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

void VerifyReportIsReady() {
  const char* output_path = std::getenv("PTI_PC_SAMPLING_OUTPUT");
  if (output_path == nullptr) {
    std::cerr << "PC sampling report unavailable to application exit handlers" << std::endl;
    return;
  }

  std::ifstream report_file(output_path);
  const std::string report{std::istreambuf_iterator<char>(report_file),
                           std::istreambuf_iterator<char>()};
  if (report.find("PC Sampling collection stopped successfully") == std::string::npos) {
    std::cerr << "PC sampling report unavailable to application exit handlers" << std::endl;
    return;
  }
  std::cerr << "PC sampling report available to application exit handlers" << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }

  if (std::strcmp(argv[1], "quick-exit") == 0 || std::strcmp(argv[1], "std-quick-exit") == 0) {
    std::at_quick_exit(VerifyReportIsReady);
  } else {
    std::atexit(VerifyReportIsReady);
  }
  if (std::strcmp(argv[1], "exit") == 0) {
    ::exit(EXIT_SUCCESS);
  }
  if (std::strcmp(argv[1], "std-exit") == 0) {
    std::exit(EXIT_SUCCESS);
  }
  if (std::strcmp(argv[1], "quick-exit") == 0) {
    ::quick_exit(EXIT_SUCCESS);
  }
  if (std::strcmp(argv[1], "std-quick-exit") == 0) {
    std::quick_exit(EXIT_SUCCESS);
  }
  return EXIT_FAILURE;
}
