
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNIEVENT_H
#define PTI_TOOLS_UNITRACE_UNIEVENT_H

enum EVENT_TYPE {
  EVENT_NULL = 0,
  EVENT_DURATION_START,
  EVENT_DURATION_END,
  EVENT_FLOW_SOURCE,
  EVENT_FLOW_SINK,
  EVENT_COMPLETE,
  EVENT_MARK,
};

typedef struct HostEventRecord_ {
  uint64_t id_;
  uint64_t start_time_;
  uint64_t end_time_;
  std::string name_;
  API_TRACING_ID api_id_;
  EVENT_TYPE type_;
} HostEventRecord;



#endif // PTI_TOOLS_UNITRACE_UNIEVENT_H
