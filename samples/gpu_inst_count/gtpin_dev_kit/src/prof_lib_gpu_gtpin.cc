
#include "prof_lib_gpu_gtpin.hpp"

#include <iostream>

/**
 * @file Implementaion of profiler classes
 */

namespace gtpin_prof {

using namespace gtpin;

GTPinProfilerBase::GTPinProfilerBase(std::shared_ptr<GTPinDataWriterBase> const writer,
                                     std::shared_ptr<GTPinFilterBase> const filter)
    : m_writer(writer), m_filter(filter) {}

PROF_STATUS GTPinProfilerBase::Start() {
  PTI_ASSERT((tool != nullptr) && "Tool was not created. Error during profiler creation");

  /// Set common GTPin knobs
  auto commonKnobs = tool->SetCommonGtpinKnobs();
  ConfigureGTPin(commonKnobs.size(), commonKnobs.data());

  /// Set hardware filter
  /// TODO: ("Set up HW filter")
  // m_filter->GetHWProfileMask()

  /// Set tool-specific knobs
  auto knobs = tool->SetGtpinKnobs();
  ConfigureGTPin(knobs.size(), knobs.data());

  toolHandle = GTPin_GetCore()->RegisterTool(*tool);

  if (toolHandle == nullptr) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
    PTI_ASSERT((toolHandle != nullptr) && "Tool was not registered");
  }

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GTPinProfilerBase::Stop() {
  tool->RunWriter();

  // Unregister tool
  bool result = GTPin_GetCore()->UnregisterTool(toolHandle);
  if (!result) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
    PTI_ASSERT(result && "Failed to unregister tool");
  }

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GTPinProfilerBase::GetStatus() {
  /// TODO: remove or implement status functionality
  return PROF_STATUS_SUCCESS;
}

}  // namespace gtpin_prof
