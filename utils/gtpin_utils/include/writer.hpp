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
  WriterBase(const std::vector<WriterBaseSPtr> writers);
  virtual ~WriterBase() = default;

  /**
   * @brief Initialize the writer.
   *
   * This function is called at least once before the first call to Write().
   *
   * @return true if the writer is successfully initialized, false otherwise.
   */
  virtual bool Init();

  /**
   * @brief Write data.
   *
   * This function is responsible for writing the profiler data.
   *
   * @param res - The profiler data to write.
   */
  virtual void Write(const ApplicationDataSPtr res);

 protected:
  /**
   * @brief Write application data.
   *
   * This function is responsible for writing the application data.
   *
   * @param res - The application data to write.
   * @return true if no need to continue writing the application data with default flow.
   */
  virtual bool WriteApplicationData(const ApplicationDataSPtr res);

  /**
   * @brief Write kernel data.
   *
   * This function is responsible for writing the kernel data.
   *
   * @param res - The application data to write.
   * @param kernelData - The kernel data to write.
   * @return true if no need to continue writing the data with default flow.
   */
  virtual bool WriteKernelData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData);

  /**
   * @brief Write invocation data.
   *
   * This function is responsible for writing the invocation data.
   *
   * @param res - The application data to write.
   * @param kernelData - The kernel data to write.
   * @param invocationData - The invocation data to write.
   * @return true if no need to continue writing the data with default flow.
   */
  virtual bool WriteInvocationData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                                   const InvocationDataSPtr invocationData);

  /**
   * @brief Write result data.
   *
   * This function is responsible for writing the result data.
   *
   * @param res - The application data to write.
   * @param kernelData - The kernel data to write.
   * @param invocationData - The invocation data to write.
   * @param resultData - The result data to write.
   * @param resultDataCommon - The common result data to write.
   * @param tileId - The tile ID.
   * @return true if no need to continue writing the data with default flow.
   */
  virtual bool WriteResultData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                               const InvocationDataSPtr invocationData,
                               const ResultDataSPtr resultData,
                               const ResultDataCommonSPtr resultDataCommon, size_t tileId);

 private:
  const std::vector<WriterBaseSPtr> m_writers;
};

/**
 * @brief The StreamWriter class is responsible for writing data to an output stream.
 */
class StreamWriter {
 public:
  StreamWriter(std::ostream& stream);
  virtual ~StreamWriter() = default;

 protected:
  /**
   * @brief Returns the underlying output stream.
   *
   * @return A reference to the output stream.
   */
  inline std::ostream& GetStream() { return m_stream; }

  std::ostream& m_stream;
};

/**
 * @brief Base class for writing text data.
 *
 * This class inherits from StreamWriter and WriterBase, and provides a base implementation for
 * writing text data.
 */
class TxtWriterBase : public StreamWriter, public virtual WriterBase {
 public:
  using StreamWriter::StreamWriter;
  virtual ~TxtWriterBase() = default;

  /**
   * @brief Writes the given application data.
   *
   * This function is used to write the provided application data to a stream.
   *
   * @param res The application data to be written.
   */
  void Write(const ApplicationDataSPtr res) override;
};

/**
 * @brief Base class for writing JSON data.
 *
 * This class inherits from StreamWriter and WriterBase, and provides a base implementation for
 * writing text data.
 */
class JsonWriterBase : public StreamWriter, public virtual WriterBase {
 public:
  using StreamWriter::StreamWriter;
  virtual ~JsonWriterBase() = default;

  /**
   * @brief Writes the given application data.
   *
   * This function is used to write the provided application data to a stream.
   *
   * @param res The application data to be written.
   */
  void Write(const ApplicationDataSPtr res) override;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_WRITER_H
