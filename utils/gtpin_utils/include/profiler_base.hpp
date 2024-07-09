//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_PROFILER_BASE_H
#define PTI_GTPIN_PROFILER_BASE_H

#include <memory>

#include "control.hpp"
#include "def_gpu.hpp"
#include "writer.hpp"

/**
 * @file profiler_base.hpp
 * @brief This file contains the declaration of the GTPinProfilerBase class.
 */

namespace gtpin_prof {

/**
 * @class GTPinProfilerBase
 * @brief The base class for the GTPin profiler.
 *
 * The GTPinProfilerBase class describes the behavior of the profiling tool. It defines
 * the functions and behavior that are used by the analyzer.
 */
class GTPinProfilerBase {
 public:
  /**
   * @brief Constructs a GTPinProfilerBase object.
   * @param writer A shared pointer to the WriterBase object used for writing profiling data.
   * @param control A shared pointer to the ControlBase object used to control of profiler.
   */
  GTPinProfilerBase(const WriterBaseSPtr writer,
                    const ControlBaseSPtr control = std::make_shared<DefaultControl>()){};

  virtual ~GTPinProfilerBase() = default;

  /**
   * @brief Starts the profiling.
   * @return The status of the operation.
   */
  virtual PROF_STATUS Start() = 0;

  /**
   * @brief Gets the status of the profiling.
   * @return The status of the profiling.
   */
  virtual PROF_STATUS Status() = 0;

  /**
   * @brief Stops the profiling.
   * @return The status of the profiling operation.
   */
  virtual PROF_STATUS Stop() = 0;

  /**
   * @brief Gets the last error message.
   * @return The last error message.
   */
  virtual std::string LastError() = 0;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_PROFILER_BASE_H
