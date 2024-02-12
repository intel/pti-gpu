//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "writer.hpp"

#include "def_gpu_gtpin.hpp"

/**
 * @file Implementaion of default writers
 */

namespace gtpin {
namespace gtpin_prof {

/**
 * StreamGTPinDataWriter implementation
 */

StreamGTPinDataWriter::StreamGTPinDataWriter(std::ostream& stream) {
  sh = new StreamHolder(stream);
};

// std::ostream& StreamGTPinDataWriter::GetStream() {
//     if (sh == nullptr) {
//         bool result = this->Init();
//         PTI_ASSERT(result != false && sh != nullptr && "Error during writer
//         initialization")
//     }
//     return sh->m_stream;
// };

StreamGTPinDataWriter::StreamHolder::StreamHolder(std::ostream& stream) : m_stream(stream){};

StreamGTPinDataWriter::StreamHolder::~StreamHolder() { m_stream << std::flush; };

/**
 * DefaultGTPinWriter implementation
 */

void DefaultGTPinWriter::Write(const std::shared_ptr<ProfilerData> res) { return; }

/**
 * MultipleGTPinWriter implementation
 */

MultipleGTPinWriter::MultipleGTPinWriter(std::vector<std::shared_ptr<GTPinDataWriterBase>> writers)
    : m_writers(writers){};

bool MultipleGTPinWriter::Init() {
  bool res = true;
  for (auto writer : m_writers) {
    if (writer == nullptr) {
      continue;
    }
    res &= writer->Init();
  }
  return res;
}

void MultipleGTPinWriter::Write(const std::shared_ptr<ProfilerData> res) {
  for (auto writer : m_writers) {
    if (writer == nullptr) {
      continue;
    }
    writer->Write(res);
  }
};

/**
 * DefaultTxtGTPinWriter implementation
 */

void DefaultTxtGTPinWriter::Write(const std::shared_ptr<ProfilerData> res) {
  WriteToolProfilerData(res);  // write tool specific application data
  GetStream() << "\n\n========================================"
                 "========================================\n";
  GetStream() << res->toolName << " report\n";
  for (auto k : res->kernels) {
    GetStream() << "----------------------------------------"
                   "----------------------------------------\n";
    GetStream() << std::setw(15) << "Kernel name: " << k.second->kernelName << "\n";
    GetStream() << std::setw(15) << "runs: " << k.second->totalRuns << "\n";
    WriteToolKernelData(res, k.second);  // print tool specific kernel data'
    for (auto invoc : k.second->invocations) {
      GetStream() << std::setw(20) << "run: " << invoc.second->globalRunNum << "\n";
      WriteToolInvocationData(res, k.second, invoc.second);  // print tool specific invocation data
      for (auto data : invoc.second->data) {
        WriteToolResultData(res, k.second, invoc.second,
                            data);  // print tool specific result data
      }
    }
    GetStream() << "\n";
  }
  GetStream() << "========================================"
                 "========================================\n";
}

/**
 * DefaultJsonGTPinWriter implementation
 */

void DefaultJsonGTPinWriter::Write(const std::shared_ptr<ProfilerData> res) {
  GetStream() << "{";
  WriteToolProfilerData(res);  // write tool specific application data
  GetStream() << "\"kernels\":{";
  bool firstK = true;
  for (auto k : res->kernels) {
    if (!firstK) {
      GetStream() << ",";
    }
    firstK = false;
    GetStream() << "\"" << k.first << "\":{";
    GetStream() << "\"name\":\"" << k.second->kernelName << "\",";
    GetStream() << "\"runs\":" << k.second->totalRuns << ",";
    WriteToolKernelData(res, k.second);  // print tool specific kernel data
    GetStream() << "\"invocations\":{";
    bool firstI = true;
    for (auto invoc : k.second->invocations) {
      if (!firstI) {
        GetStream() << ",";
      }
      firstI = false;
      GetStream() << "\"" << invoc.first << "\":{";
      GetStream() << "\"id\":" << invoc.second->kernelId << ",";
      GetStream() << "\"run\":" << invoc.second->runNum << ",";
      GetStream() << "\"globalRun\":" << invoc.second->globalRunNum << ",";
      WriteToolInvocationData(res, k.second, invoc.second);  // print tool specific invocation data
      GetStream() << "\"data\":[";
      bool firstR = true;
      for (auto data : invoc.second->data) {
        if (!firstR) {
          GetStream() << ",";
        }
        firstR = false;
        GetStream() << "{";
        WriteToolResultData(res, k.second, invoc.second,
                            data);  // print tool specific result data
        GetStream() << "}";
      }
      GetStream() << "]";  // data
      GetStream() << "}";  // per invocation data
    }
    GetStream() << "}";  // invocations
    GetStream() << ",\"asm\": [";
    for (auto asmRecord : k.second->origAsm) {
      GetStream() << "\"" << asmRecord.asmLineOrig << "\",";
    }
    GetStream() << "],";
    GetStream() << "}";  // per kernelId data
  }
  GetStream() << "}";  // kernels
  GetStream() << "}\n";
  GetStream() << std::flush;
}

/**
 * DefaultCsvGTPinWriter implementation
 */

void DefaultCsvGTPinWriter::Write(const std::shared_ptr<ProfilerData> res) {
  /// TODO: ("Implement CSV writer")
}

}  // namespace gtpin_prof
}  // namespace gtpin
