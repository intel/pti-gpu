//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file tool.cpp
 * @brief Implements the GTPinTool class which is responsible for instrumenting and profiling
 * kernels.
 *
 * This file contains the implementation of the GTPinTool class. The GTPinTool class is used to
 * instrument and profile kernels using the GTPin framework. It provides functions for analyzing
 * kernels, allocating resources, instrumenting kernels, and collecting profiling data.
 */

#include "tool.hpp"

#include <algorithm>

using namespace gtpin_prof;

GTPinTool::GTPinTool(const ToolFactorySPtr factory)
    : m_factory(factory),
      m_control(factory->GetControl()),
      m_applicationData(factory->MakeApplicationData()),
      m_globalRun(0) {
  PTI_ASSERT(m_control != nullptr);
}

PROF_STATUS GTPinTool::RunWriter(const WriterBaseSPtr writer) const {
  writer->Write(m_applicationData);
  return PROF_STATUS::SUCCESS;
}

std::vector<const char*> GTPinTool::GetCommonGTPinKnobs() const {
  // return std::vector<const char*>{"--allow_sregs", "0",
  //  "-d"};
  return std::vector<const char*>{"--no_empty_profile_dir"};
}

std::vector<const char*> GTPinTool::GetGTPinKnobs() const {
  // return std::vector<const char*>{"-d"};
  return std::vector<const char*>();
}

const KernelRun GTPinTool::GetGlobalRun() const { return m_globalRun; }

const ApplicationDataSPtr GTPinTool::GetProfilingData() const { return m_applicationData; }

void GTPinTool::OnKernelBuild(gtpin::IGtKernelInstrument& instrumentor) {
  PROF_STATUS status;

  KernelDataSPtr kernelData = CreateKernelInStorage(instrumentor);

  if (!m_control->ShouldInstrument(instrumentor)) {
    return;
  }

  status = AnalyzeKernel(kernelData, instrumentor);
  if (status == PROF_STATUS::NOTHING_TO_INSTRUMENT || kernelData->GetSiteOfInstrumentNum() == 0) {
    return;
  }

  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to analyze kernel");

  status = AllocateResources(kernelData, instrumentor);
  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to allocate resources");

  status = Instrument(kernelData, instrumentor);
  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to instrument kernel");
}

void GTPinTool::OnKernelRun(gtpin::IGtKernelDispatch& dispatcher) {
  PROF_STATUS status;

  this->IncGlobalRuns();

  /// Get kernel from storage. Warning if kernel not in storage, but not profile
  PTI_ASSERT(this->IsKernelInStorage(dispatcher.Kernel().Id()) &&
             "Trying to analyze kernel that was not built");

  KernelDataSPtr kernelData = this->GetKernel(dispatcher.Kernel().Id());
  this->IncKernelRuns(kernelData);

  // Check if GTPin failed to instrument kernel
  if (!dispatcher.Kernel().IsInstrumented()) return;

  // Check that kernel was not instrumented. Nothing to instrument -> no ProfileBuffer allocation
  if (!kernelData->m_profileArray.IsAllocated()) return;

  auto execDescr =
      KernelExecDescriptor(dispatcher, this->GetGlobalRun(), kernelData->GetKernelRun());

  if (!m_control->ShouldProfileEnqueue(execDescr)) {
    return;
  }

  status = this->InitProfileData(kernelData, dispatcher);
  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to initiate result");

  status = this->InitBuffer(kernelData, dispatcher);
  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to initiate array");

  dispatcher.SetProfilingMode(true);
}

void GTPinTool::OnKernelComplete(gtpin::IGtKernelDispatch& dispatcher) {
  PROF_STATUS status;

  /// Get kernel from storage. Warning if kernel not in storage, but not profile
  PTI_ASSERT(this->IsKernelInStorage(dispatcher.Kernel().Id()) &&
             "Trying to process kernel that was not built");

  KernelDataSPtr kernelData = this->GetKernel(dispatcher.Kernel().Id());

  if (!dispatcher.Kernel().IsInstrumented() || !dispatcher.IsProfilingEnabled()) {
    return;
  }

  PTI_ASSERT(dispatcher.IsCompleted() == true);

  status = this->ReadProfileData(kernelData, dispatcher, m_factory);
  PTI_ASSERT(PROF_STATUS::SUCCESS == status && "Fail to read data");

  SetInvocationCollected(kernelData, dispatcher);
}

PROF_STATUS GTPinTool::PostProcData(KernelDataSPtr kernel, InvocationDataSPtr invocationResult) {
  return PROF_STATUS::SUCCESS;
}

PROF_STATUS GTPinTool::AllocateResources(KernelDataSPtr kernelData,
                                         gtpin::IGtKernelInstrument& instrumentor) {
  PTI_ASSERT(kernelData->IsRecordSizeSet() &&
             "Record size not initialized. Check \"AnalyzeKernel\" function");
  PTI_ASSERT((kernelData->GetRecordSize() != 0) &&
             "Zero record size. Check \"AnalyzeKernel\" function");
  PTI_ASSERT((kernelData->GetSiteOfInstrumentNum() != 0) &&
             "Zero record num. Check \"AnalyzeKernel\" function");

  if (kernelData->GetBucketsNum() == 0) {
    SetDefaultBuckets(kernelData, instrumentor);
  }

  kernelData->m_profileArray =
      gtpin::GtProfileArray(kernelData->GetRecordSize(), kernelData->GetSiteOfInstrumentNum(),
                            kernelData->GetBucketsNum());
  gtpin::IGtProfileBufferAllocator& allocator = instrumentor.ProfileBufferAllocator();
  if (!kernelData->m_profileArray.Allocate(allocator)) {
    return PROF_STATUS::ERROR;
  }

  return PROF_STATUS::SUCCESS;
}

PROF_STATUS GTPinTool::InitProfileData(KernelDataSPtr kernelData,
                                       gtpin::IGtKernelDispatch& dispatcher) {
  /// Create new invocation
  auto execDescr =
      KernelExecDescriptor(dispatcher, this->GetGlobalRun(), kernelData->GetKernelRun());
  kernelData->m_invocations[dispatcher.DispatchId()] = m_factory->MakeInvocationData(execDescr);
  auto invocation = kernelData->m_invocations[dispatcher.DispatchId()];
  PTI_ASSERT(invocation != nullptr && "Invocation data was not initialized");

  for (const auto& rdc : kernelData->GetResultDataCommon()) {
    invocation->m_resultData.push_back(m_factory->MakeResultData(rdc));
    PTI_ASSERT(invocation->m_resultData.back() != nullptr && "Fail to add ResultData");
  }
  PTI_ASSERT(invocation->m_resultData.size() == kernelData->GetResultsNum() &&
             "Invalid number of result data objects");

  return PROF_STATUS::SUCCESS;
}

PROF_STATUS GTPinTool::InitBuffer(KernelDataSPtr kernelData, gtpin::IGtKernelDispatch& dispatcher) {
  gtpin::IGtProfileBuffer* buffer = dispatcher.CreateProfileBuffer();
  PTI_ASSERT(buffer != nullptr && "Profile buffer was not created");

  bool success = kernelData->m_profileArray.Initialize(*buffer);

  return (success) ? PROF_STATUS::SUCCESS : PROF_STATUS::ERROR;
}

PROF_STATUS GTPinTool::ReadProfileData(KernelDataSPtr kernelData,
                                       gtpin::IGtKernelDispatch& dispatcher,
                                       const ToolFactorySPtr factory) {
  PROF_STATUS status;

  const gtpin::IGtProfileBuffer* buffer = dispatcher.GetProfileBuffer();
  PTI_ASSERT((buffer != nullptr) && "Profile kernel was not found");

  auto invocation = kernelData->m_invocations[dispatcher.DispatchId()];
  PTI_ASSERT(invocation != nullptr && "Invocation data was not initialized");

  char* recordChar = new char[m_factory->GetRecordSize()];
  RawRecord* record = reinterpret_cast<RawRecord*>(recordChar);

  for (size_t i = 0; i < kernelData->GetSiteOfInstrumentNum(); i++) {
    for (uint32_t threadBucket = 0; threadBucket < kernelData->m_profileArray.NumThreadBuckets();
         ++threadBucket) {
      if (!kernelData->m_profileArray.Read(*buffer, record, i, 1, threadBucket)) {
        delete[] recordChar;
        return PROF_STATUS::ERROR;
      } else {
        PTI_ASSERT((record != nullptr) && "Record is corrupted");
        auto site = kernelData->GetSiteOfInstrument(i);
        for (const auto& resultData : GetResultDataForSiteOfInstrument(invocation, site)) {
          status = Accumulate(kernelData, resultData, site, record);
          PTI_ASSERT((PROF_STATUS::SUCCESS == status) && "Fail to accumulate result data");
        }
      }
    }
  }

  delete[] recordChar;

  status = this->PostProcData(kernelData, invocation);
  PTI_ASSERT((PROF_STATUS::SUCCESS == status) && "Fail to post process data");

  return PROF_STATUS::SUCCESS;
}

/// KernelData storage functions
KernelDataSPtr GTPinTool::CreateKernelInStorage(const gtpin::IGtKernelInstrument& instrumentor) {
  KernelId kernelId = instrumentor.Kernel().Id();

  // Check if kernel exist in storage
  PTI_ASSERT(this->IsKernelInStorage(kernelId) == false && "Kernel is already instrumented");

  // Create kernel data in storage
  auto kernelData = m_factory->MakeKernelData(instrumentor);
  SetRecordSize(kernelData, m_factory->GetRecordSize());

  m_applicationData->m_kernels.insert({kernelId, kernelData});

  return kernelData;
}

bool GTPinTool::IsKernelInStorage(const KernelId& kernelId) const {
  return m_applicationData->m_kernels.find(kernelId) != m_applicationData->m_kernels.end();
}

KernelDataSPtr GTPinTool::GetKernel(const KernelId& kernelId) const {
  return m_applicationData->m_kernels.at(kernelId);
}

size_t GTPinTool::GetKernelsNum() { return m_applicationData->m_kernels.size(); }
std::vector<KernelId> GTPinTool::GetKernelIds() {
  std::vector<KernelId> ids;
  for (const auto& k : m_applicationData->m_kernels) ids.push_back(k.first);
  return ids;
}
size_t GTPinTool::AddResultData(KernelDataSPtr kernelData, ResultDataCommonSPtr resultDataCommon) {
  kernelData->m_resultDataCommon.push_back(resultDataCommon);
  return kernelData->m_resultDataCommon.size() - 1;
}
void GTPinTool::IncKernelRuns(KernelDataSPtr kernelData) { kernelData->m_kernelRuns++; }
void GTPinTool::SetRecordSize(KernelDataSPtr kernelData, uint32_t recordSize) {
  kernelData->m_recordSize = recordSize;
}
void GTPinTool::SetBucketsNum(KernelDataSPtr kernelData, size_t buckets) {
  kernelData->m_buckets = buckets;
}
void GTPinTool::SetDefaultBuckets(KernelDataSPtr kernelData,
                                  const gtpin::IGtKernelInstrument& instrumentor) {
  this->SetBucketsNum(kernelData, instrumentor.Kernel().GenModel().MaxThreadBuckets());
}
void GTPinTool::AddSiteOfInstrument(KernelDataSPtr kernelData,
                                    SiteOfInstrumentSPtr siteOfInstrument) {
  kernelData->m_sitesOfInterest.push_back(siteOfInstrument);
}
SiteOfInstrumentSPtr GTPinTool::GetSiteOfInstrument(KernelDataSPtr kernelData, size_t idx) {
  return kernelData->m_sitesOfInterest.at(idx);
}
gtpin::GtProfileArray& GTPinTool::GetProfileArray(KernelDataSPtr kernelData) {
  return kernelData->m_profileArray;
}
void GTPinTool::MapResultData(SiteOfInstrumentSPtr siteOfInstrument, size_t resultDataIdx) {
  siteOfInstrument->m_results.push_back(resultDataIdx);
}
void GTPinTool::SetInvocationCollected(KernelDataSPtr kernelData,
                                       gtpin::IGtKernelDispatch& dispatcher) {
  kernelData->m_invocations[dispatcher.DispatchId()]->m_collected = true;
}

std::vector<ResultDataSPtr> GTPinTool::GetResultDataForSiteOfInstrument(
    InvocationDataSPtr invocation, SiteOfInstrumentSPtr siteOfInstrument) {
  std::vector<ResultDataSPtr> resultData;
  for (const auto& idx : siteOfInstrument->m_results) {
    resultData.push_back(invocation->GetResultData(idx));
  }
  return resultData;
}
const ToolFactorySPtr GTPinTool::GetFactory() { return m_factory; }
const ControlBaseSPtr GTPinTool::GetControl() { return m_control; }

void GTPinTool::IncGlobalRuns() { m_globalRun++; }
