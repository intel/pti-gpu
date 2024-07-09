//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file profiler.cpp
 * @brief Implementation of the GTPinProfiler class.
 *
 * This file contains the implementation of the GTPinProfiler class, which is responsible for
 * starting, stopping, and checking the status of the GTPin profiler tool. It also provides
 * functionality for configuring GTPin knobs and handling errors.
 */

#include "profiler.hpp"

#include "api/gtpin_api.h"
#include "def_gpu.hpp"
#include "knob_parser.h"

using namespace gtpin_prof;

GTPinProfiler::GTPinProfiler(const WriterBaseSPtr writer, const ControlBaseSPtr control)
    : GTPinProfilerBase(writer, control), m_writer(writer), m_control(control) {}

PROF_STATUS GTPinProfiler::Start() {
  PTI_ASSERT(m_gtpinTool == nullptr && "Tool was not created. Error during profiler creation");

  auto factory = GetFactory(m_control);
  m_gtpinTool = factory->MakeGTPinTool();

  /// Set common GTPin knobs
  auto commonKnobs = m_gtpinTool->GetCommonGTPinKnobs();
  ConfigureGTPin(commonKnobs.size(), commonKnobs.data());

  /// Set tool-specific knobs
  auto knobs = m_gtpinTool->GetGTPinKnobs();
  ConfigureGTPin(knobs.size(), knobs.data());

  m_gtpinToolHandle = gtpin::GTPin_GetCore()->RegisterTool(*m_gtpinTool);

  if (m_gtpinToolHandle == nullptr) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
    PTI_ASSERT((m_gtpinToolHandle != nullptr) && "Tool was not registered");
  }

  return PROF_STATUS::SUCCESS;
}

PROF_STATUS GTPinProfiler::Status() {
  return m_gtpinTool == nullptr ? PROF_STATUS::STOP : PROF_STATUS::ACTIVE;
}

PROF_STATUS GTPinProfiler::Stop() {
  PTI_ASSERT(this->Status() == PROF_STATUS::ACTIVE);

  auto status = m_gtpinTool->RunWriter(m_writer);
  PTI_ASSERT(status == PROF_STATUS::SUCCESS);

  PTI_ASSERT(m_gtpinToolHandle != nullptr);
  bool result = gtpin::GTPin_GetCore()->UnregisterTool(m_gtpinToolHandle);
  if (!result) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
    PTI_ASSERT(result && "Failed to unregister tool");
  }
  m_gtpinToolHandle = nullptr;
  m_gtpinTool = nullptr;

  return PROF_STATUS::SUCCESS;
}

std::string GTPinProfiler::LastError() {
  return GTPIN_LAST_ERROR_STR;  // Return appropriate error message
}
