//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_WRITER_H
#define PTI_GTPIN_WRITER_H

#include <api/gtpin_api.h>

#include <memory>
#include <vector>

#include "results.hpp"

/**
 * @file This file describes writer interface and several default writers, which
 * can be extended for tool specific output
 */

namespace gtpin_prof {

/**
 * @class WriterBase
 * @brief Base class for writers that write profiler data.
 *
 * This class serves as the base class for all writer classes that are responsible for writing
 * profiler data.
 */
class WriterBase {
 public:
  WriterBase() = default;
  virtual ~WriterBase() = default;

  /**
   * @brief Write data.
   *
   * This function is responsible for writing the profiler data.
   *
   * @param res - The profiler data to write.
   */
  virtual void Write(const ApplicationDataSPtr res) const = 0;

  /**
   * @brief Initialize the writer.
   *
   * This function is called at least once before the first call to Write().
   *
   * @return true if the writer is successfully initialized, false otherwise.
   */
  virtual bool Init();
};

/**
 * @class MultipleWriter
 * @brief Class that allows using multiple writers.
 *
 * This class allows using multiple writer objects to write profiler data.
 */
class MultipleWriter : public WriterBase {
 public:
  /**
   * @brief Constructs a MultipleWriter object with the given list of writers.
   *
   * @param writers - The list of writer objects.
   */
  MultipleWriter(std::vector<WriterBaseSPtr> writers);
  virtual ~MultipleWriter() = default;

  /**
   * @brief Initialize all writers in the list.
   *
   * This function initializes all the writer objects in the list of writers.
   *
   * @return true if all writers are successfully initialized, false if any writer fails to
   * initialize.
   */
  bool Init() final;

  /**
   * @brief Write data using all writers.
   *
   * This function writes the profiler data using all the writer objects in the list.
   *
   * @param res - The profiler data to write.
   */
  void Write(const ApplicationDataSPtr res) const final;

 private:
  const std::vector<WriterBaseSPtr> m_writers;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_WRITER_H
