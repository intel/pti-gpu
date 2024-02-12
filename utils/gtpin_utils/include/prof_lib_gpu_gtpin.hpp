//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_PROFILER_H
#define PTI_GTPIN_PROFILER_H

#include <api/gtpin_api.h>

#include <memory>

#include "def_gpu_gtpin.hpp"
#include "filter.hpp"
#include "gtpin_capsules.hpp"
#include "gtpin_tool.hpp"
#include "knob_parser.h"
#include "results_gtpin.hpp"
#include "writer.hpp"

namespace gtpin {
namespace gtpin_prof {

/**
 * @brief GTPinProfilerBase describes of behaviour of profiling tool. It describes
 * functions and behaviour that is used n analyzer
 */
class GTPinProfilerBase {
 public:
  GTPinProfilerBase(
      const std::shared_ptr<GTPinDataWriterBase> writer = std::make_shared<DefaultGTPinWriter>(),
      const std::shared_ptr<GTPinFilterBase> filter = std::make_shared<DefaultGTPinFilter>());
  virtual ~GTPinProfilerBase() = default;

  PROF_STATUS Start();
  PROF_STATUS Stop();

 protected:
  std::shared_ptr<GTPinTool> tool;

 private:
  GTPinProfilerBase(const GTPinProfilerBase&) = delete;
  GTPinProfilerBase& operator=(const GTPinProfilerBase&) = delete;

  inline const std::shared_ptr<GTPinDataWriterBase> GetWriter() const { return m_writer; }
  inline const std::shared_ptr<GTPinFilterBase> GetFilter() const { return m_filter; }

  const std::shared_ptr<GTPinDataWriterBase> m_writer;
  const std::shared_ptr<GTPinFilterBase> m_filter;
  GtToolHandle toolHandle = nullptr;
};

}  // namespace gtpin_prof
}  // namespace gtpin
#endif  // PTI_GTPIN_PROFILER_H
