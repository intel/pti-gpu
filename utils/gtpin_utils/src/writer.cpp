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
WriterBase::WriterBase(const std::vector<WriterBaseSPtr> writers) : m_writers(std::move(writers)) {}

bool WriterBase::Init() {
  for (const auto& writer : m_writers) {
    if (!writer->Init()) {
      return false;
    }
  }
  return true;
}

void WriterBase::Write(const ApplicationDataSPtr res) {
  for (const auto& writer : m_writers) {
    writer->Write(res);
  }

  if (WriteApplicationData(res)) return;
  for (const auto& kernelDataPair : res->GetKernels()) {
    auto kernelData = kernelDataPair.second;
    if (WriteKernelData(res, kernelData)) continue;
    for (const auto& invocationDataPair : kernelData->GetInvocations()) {
      auto invocationData = invocationDataPair.second;
      if (WriteInvocationData(res, kernelData, invocationData)) continue;
      for (size_t idx = 0; idx < kernelData->GetResultsNum(); idx++) {
        for (size_t tileId = 0; tileId < kernelData->GetCollectedTilesNum(); tileId++) {
          WriteResultData(res, kernelData, invocationData,
                          invocationData->GetResultData(tileId, idx),
                          kernelData->GetResultDataCommon(idx), tileId);
        }
      }
    }
  }
}

bool WriterBase::WriteApplicationData(const ApplicationDataSPtr res) { return false; }

bool WriterBase::WriteKernelData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData) {
  return false;
}

bool WriterBase::WriteInvocationData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                                     const InvocationDataSPtr invocationData) {
  return false;
}

bool WriterBase::WriteResultData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                                 const InvocationDataSPtr invocationData,
                                 const ResultDataSPtr resultData,
                                 const ResultDataCommonSPtr resultDataCommon, size_t tileId) {
  return false;
}

/**
 * TxtWriterBase implementation
 */
void TxtWriterBase::Write(const ApplicationDataSPtr res) {
  if (WriteApplicationData(res)) return;
  for (const auto& kernelDataPair : res->GetKernels()) {
    auto kernelData = kernelDataPair.second;
    GetStream() << "=== " << kernelData->GetKernelName() << " ";
    GetStream() << "(runs " << kernelData->GetKernelRun() << " times";
    size_t collectedTimes = 0;
    for (const auto& invocationDataPair : kernelData->GetInvocations()) {
      if (invocationDataPair.second->IsCollected()) collectedTimes++;
    }
    if (collectedTimes != kernelData->GetInvocations().size())
      GetStream() << ", collected " << collectedTimes << " times";
    GetStream() << ")\n";
    if (WriteKernelData(res, kernelData)) continue;

    for (const auto& invocationDataPair : kernelData->GetInvocations()) {
      auto invocationData = invocationDataPair.second;
      if (WriteInvocationData(res, kernelData, invocationData)) continue;

      GetStream() << "Run: " << invocationData->GetRunNum();
      GetStream() << ", Global run: " << invocationData->GetGlobalRunNum();
      GetStream() << ", Profiled: " << invocationData->IsCollected();

      for (size_t tileId = 0; tileId < kernelData->GetCollectedTilesNum(); tileId++) {
        for (size_t idx = 0; idx < kernelData->GetResultsNum(); idx++) {
          WriteResultData(res, kernelData, invocationData,
                          invocationData->GetResultData(tileId, idx),
                          kernelData->GetResultDataCommon(idx), tileId);
        }
      }
    }
  }
  GetStream() << "\n\n";
}

/**
 * StreamWriter implementation
 */
StreamWriter::StreamWriter(std::ostream& stream) : m_stream(stream) {}

/**
 * JsonWriterBase implementation
 */
void JsonWriterBase::Write(const ApplicationDataSPtr res) {
  GetStream() << "{\n";
  GetStream() << "\"app_name\":\"" << res->GetApplicationName() << "\"";
  if (WriteApplicationData(res)) return;
  GetStream() << ",\"kernels\":[\n";
  bool firstKernel = true;
  for (const auto& kernelDataPair : res->GetKernels()) {
    auto kernelData = kernelDataPair.second;
    if (!firstKernel) GetStream() << ",";
    firstKernel = false;
    GetStream() << "{";
    GetStream() << "\"kernel_name\":\"" << kernelData->GetKernelName() << "\"";
    GetStream() << ",\"runs\":" << kernelData->GetKernelRun();
    GetStream() << ",\"id\":" << kernelData->GetKernelId();
    GetStream() << ",\"results_num\":" << kernelData->GetResultsNum();
    GetStream() << ",\"asm\":[";
    bool firstAsm = true;
    for (const auto& asm_point : kernelData->GetOrigAsm()) {
      if (!firstAsm) GetStream() << ",";
      firstAsm = false;
      GetStream() << "{";
      GetStream() << "\"offset\":" << asm_point.GetInstructionOffset();
      GetStream() << ",\"asm\":\"" << asm_point.GetAsmLineOrig() << "\"";
      GetStream() << ",\"source\":{";
      GetStream() << "\"file\":\"" << asm_point.GetSourcePoint().GetFile() << "\"";
      GetStream() << ",\"line\":" << asm_point.GetSourcePoint().GetLine();
      GetStream() << ",\"column\":" << asm_point.GetSourcePoint().GetColumn();
      GetStream() << ",\"function\":\"" << asm_point.GetSourcePoint().GetFunction() << "\"";
      GetStream() << "}";  // source
      GetStream() << "}";
    }
    GetStream() << "]";  // asm
    size_t collectedTimes = 0;
    for (const auto& invocationDataPair : kernelData->GetInvocations()) {
      if (invocationDataPair.second->IsCollected()) collectedTimes++;
    }
    GetStream() << ",\"collected_runs\":" << collectedTimes;
    if (WriteKernelData(res, kernelData)) continue;
    GetStream() << ",\"invocations\":[\n";
    bool firstInvocation = true;
    for (const auto& invocationDataPair : kernelData->GetInvocations()) {
      auto invocationData = invocationDataPair.second;
      if (!firstInvocation) GetStream() << ",";
      firstInvocation = false;
      GetStream() << "{";
      GetStream() << "\"run_num\":" << invocationData->GetRunNum();
      GetStream() << ",\"global_run_num\":" << invocationData->GetGlobalRunNum();
      GetStream() << ",\"dispatch_id\":" << invocationData->GetDispatchId();
      GetStream() << ",\"is_collected\":" << invocationData->IsCollected();
      if (WriteInvocationData(res, kernelData, invocationData)) continue;
      GetStream() << ",\"tiles\":[\n";
      bool firstTile = true;
      for (size_t tileId = 0; tileId < kernelData->GetCollectedTilesNum(); tileId++) {
        if (!firstTile) GetStream() << ",";
        firstTile = false;
        GetStream() << "{";
        GetStream() << "\"results\":[";
        bool firstResult = true;
        for (size_t idx = 0; idx < kernelData->GetResultsNum(); idx++) {
          if (!firstResult) GetStream() << ",";
          firstResult = false;
          GetStream() << "{";
          WriteResultData(res, kernelData, invocationData,
                          invocationData->GetResultData(tileId, idx),
                          kernelData->GetResultDataCommon(idx), tileId);
          GetStream() << "}";
        }
        GetStream() << "]";  // results
        GetStream() << "}\n";
      }
      GetStream() << "]";  // tiles
      GetStream() << "}\n";
    }
    GetStream() << "]";  // invocations
    GetStream() << "}\n";
  }
  GetStream() << "]";  // kernels
  GetStream() << "}\n";
}
