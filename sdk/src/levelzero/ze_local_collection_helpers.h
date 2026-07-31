//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_
#define PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include "overhead_kinds.h"
#include "pti_assert.h"

// Workaround for supporting counter-based L0 events.
// https://github.com/intel/compute-runtime/blob/master/level_zero/doc/experimental_extensions/COUNTER_BASED_EVENTS.md
// These are now the default on BMG and newer platforms in 2025.3.
// preferred solution would be
// pseudo code:
//
// ```
//   if(zeCommandListInorder(command_list) || zeEventIsCounterEvent(signal_event))
//     Inject(CounterEvent() || SignalEvent()).
// ```
//
// However, these APIs do not exist yet.
// This code only works for in_order command lists.
inline bool A2AppendWaitAndSignalEvent(ze_command_list_handle_t command_list,
                                       ze_event_handle_t signal_event,
                                       ze_event_handle_t wait_event) {
  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               static_cast<const void*>(command_list), static_cast<const void*>(signal_event),
               static_cast<const void*>(wait_event));
  ze_result_t wait_res = ZE_RESULT_SUCCESS;
  {
    overhead::ScopedOverheadCollector overhead_collector{zeCommandListAppendWaitOnEvents_id};
    wait_res = zeCommandListAppendWaitOnEvents(command_list, 1U, &wait_event);
  }
  if (wait_res != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("In {}, zeCommandListAppendWaitOnEvents failed: 0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(wait_res));
    return false;
  }

  ze_result_t signal_res = ZE_RESULT_SUCCESS;
  {
    overhead::ScopedOverheadCollector overhead_collector{zeCommandListAppendSignalEvent_id};
    signal_res = zeCommandListAppendSignalEvent(command_list, signal_event);
  }
  if (signal_res != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("In {}, zeCommandListAppendSignalEvent failed: 0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(signal_res));
    return false;
  }

  return true;
}

#endif  // PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_
