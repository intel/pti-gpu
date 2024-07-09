//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file writer.cpp
 * @brief Contains the implementation of the WriterBase and MultipleWriter classes.
 *
 * This file provides the implementation of the WriterBase and MultipleWriter classes,
 * which are used for writing application data to multiple writers.
 */

#include "writer.hpp"

using namespace gtpin_prof;

/**
 * WriterBase implementation
 */
bool WriterBase::Init() { return true; }

/**
 * MultipleWriter implementation
 */
MultipleWriter::MultipleWriter(std::vector<WriterBaseSPtr> writers)
    : m_writers(std::move(writers)){};

bool MultipleWriter::Init() {
  for (const auto& writer : m_writers) {
    if (!writer->Init()) {
      return false;
    }
  }
  return true;
}

void MultipleWriter::Write(const ApplicationDataSPtr res) const {
  for (const auto& writer : m_writers) {
    writer->Write(res);
  }
}
