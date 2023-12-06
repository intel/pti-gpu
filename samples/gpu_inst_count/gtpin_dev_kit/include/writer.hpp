#ifndef PLGG_WRITER_H
#define PLGG_WRITER_H

#include <api/gtpin_api.h>

#include <memory>

#include "results_gtpin.hpp"

/**
 * @file This file describes writer interface and several default writers, which
 * can be exetended for tool specific output
 */

using namespace gtpin;

namespace gtpin_prof {

class GTPinDataWriterBase {
 public:
  GTPinDataWriterBase() = default;
  virtual ~GTPinDataWriterBase() = default;

  /// Main function, that recieves the profiler data results. The function is
  /// writer specific
  virtual void Write(const std::shared_ptr<ProfilerData> res) = 0;

  /// Init function will be called at least once before the first call of Write
  virtual bool Init() { return true; }
};

/// StreamGTPinDataWriter describes stream writers functionality
class StreamGTPinDataWriter : public GTPinDataWriterBase {
 public:
  /// stream can be defined at the construction time, or during Init function
  /// call
  StreamGTPinDataWriter() = default;
  StreamGTPinDataWriter(std::ostream& stream);
  virtual ~StreamGTPinDataWriter() = default;

 protected:
  // virtual std::ostream& GetStream();
  inline std::ostream& GetStream() { return sh->m_stream; }
  class StreamHolder {
   public:
    StreamHolder(std::ostream& stream);
    ~StreamHolder();
    std::ostream& m_stream;
  };
  StreamHolder* sh = nullptr;

  /// Specific functions that extends functionality of predefined writes. Called
  /// on each level after main writer work
  virtual void WriteToolProfilerData(const std::shared_ptr<ProfilerData> profData) {}
  virtual void WriteToolKernelData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData) {}
  virtual void WriteToolInvocationData(const std::shared_ptr<ProfilerData> profData,
                                       const std::shared_ptr<KernelData> kerData,
                                       const std::shared_ptr<InvocationData> invData) {}
  virtual void WriteToolResultData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData,
                                   const std::shared_ptr<InvocationData> invData,
                                   const std::shared_ptr<ResultData> resData) {}
};

/// Does nothing
class DefaultGTPinWriter : public GTPinDataWriterBase {
 public:
  DefaultGTPinWriter() = default;
  virtual ~DefaultGTPinWriter() = default;
  void Write(const std::shared_ptr<ProfilerData> res) final;
};

/// Allow to use several writers
class MultipleGTPinWriter : public GTPinDataWriterBase {
 public:
  MultipleGTPinWriter(std::vector<std::shared_ptr<GTPinDataWriterBase>> writers);
  virtual ~MultipleGTPinWriter() = default;
  bool Init() final;
  void Write(const std::shared_ptr<ProfilerData> res) final;

 private:
  const std::vector<std::shared_ptr<GTPinDataWriterBase>> m_writers;
};

/// Generates text, passes to ostream
class DefaultTxtGTPinWriter : public StreamGTPinDataWriter {
 public:
  using StreamGTPinDataWriter::StreamGTPinDataWriter;
  void Write(const std::shared_ptr<ProfilerData> res) final;
  virtual void WriteToolProfilerData(const std::shared_ptr<ProfilerData> profData) {}
  virtual void WriteToolKernelData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData) {}
  virtual void WriteToolInvocationData(const std::shared_ptr<ProfilerData> profData,
                                       const std::shared_ptr<KernelData> kerData,
                                       const std::shared_ptr<InvocationData> invData) {}
  virtual void WriteToolResultData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData,
                                   const std::shared_ptr<InvocationData> invData,
                                   const std::shared_ptr<ResultData> resData) {}
};

/// Generates JSON, passes to ostream
class DefaultJsonGTPinWriter : public StreamGTPinDataWriter {
 public:
  using StreamGTPinDataWriter::StreamGTPinDataWriter;
  void Write(const std::shared_ptr<ProfilerData> res) final;
  virtual void WriteToolProfilerData(const std::shared_ptr<ProfilerData> profData) {}
  virtual void WriteToolKernelData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData) {}
  virtual void WriteToolInvocationData(const std::shared_ptr<ProfilerData> profData,
                                       const std::shared_ptr<KernelData> kerData,
                                       const std::shared_ptr<InvocationData> invData) {}
  virtual void WriteToolResultData(const std::shared_ptr<ProfilerData> profData,
                                   const std::shared_ptr<KernelData> kerData,
                                   const std::shared_ptr<InvocationData> invData,
                                   const std::shared_ptr<ResultData> resData) {}
};

/// Generates CSV, passes to ostream
class DefaultCsvGTPinWriter : public StreamGTPinDataWriter {
 public:
  using StreamGTPinDataWriter::StreamGTPinDataWriter;
  void Write(const std::shared_ptr<ProfilerData> res) final;
};

}  // namespace gtpin_prof

#endif  // PLGG_WRITER_H
