//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_PROFILER_H
#define PTI_GTPIN_PROFILER_H

#include <memory>

#include "profiler_base.hpp"
#include "tool.hpp"

/**
 * @file profiler.hpp
 * @brief This file contains the declaration of the GTPinProfiler class, which implements
 * GTPinProfilerBase functionality.
 */
namespace gtpin_prof {

/**
 * @brief GTPinProfiler class, a derived class from GTPinProfilerBase, provides the functionality
 * for profiling using GTPin. It includes methods for starting, stopping, and checking the status of
 * the profiling process, as well as retrieving the last error message. It also stores the
 * associated WriterBase, ControlBase, and ToolFactory objects.
 */
class GTPinProfiler : public GTPinProfilerBase {
 public:
  /**
   * @brief Constructs a GTPinProfiler object.
   * @param writer A shared pointer to the WriterBase object used for writing profiling data.
   * @param control A shared pointer to the ControlBase object used for controlling the profiler.
   */
  GTPinProfiler(const WriterBaseSPtr writer,
                const ControlBaseSPtr control = std::make_shared<DefaultControl>());
  ~GTPinProfiler() override = default;

  /**
   * @brief Starts the profiling process.
   * @return PROF_STATUS indicating the status of the operation.
   */
  PROF_STATUS Start() final;

  /**
   * @brief Retrieves the current profiling status.
   * @return PROF_STATUS indicating the status of the profiler.
   */
  PROF_STATUS Status() final;

  /**
   * @brief Stops the profiling process.
   * @return PROF_STATUS indicating the status of the operation.
   */
  PROF_STATUS Stop() final;

  /**
   * @brief Retrieves the last error message.
   * @return A string containing the last error message.
   */
  std::string LastError() final;

 private:
  /**
   * @brief Retrieves the tool factory object.
   * Derived classes (profiler implementation) should implement this function to return a factory
   * object specific to their tool.
   * @return A shared pointer to the ToolFactory object.
   */
  virtual ToolFactorySPtr GetFactory(const ControlBaseSPtr control) = 0;

  GTPinToolSPtr m_gtpinTool = nullptr;  ///< A shared pointer to the GTPinTool object.
  gtpin::GtToolHandle m_gtpinToolHandle =
      nullptr;  ///< A handle to the GTPin tool, obtained after registration. It is used to
                ///< unregister the tool.
  const WriterBaseSPtr m_writer;    ///< A shared pointer to the WriterBase object.
  const ControlBaseSPtr m_control;  ///< A shared pointer to the ControlBase object.
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_PROFILER_H
