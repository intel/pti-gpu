
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

enum API_TYPE {
  NONE,
  MPI,
  ITT,
  CCL
};

typedef struct MpiArgs_ {
  int src_location;
  int src_tag;
  int dst_location;
  int dst_tag;
  size_t src_size;
  size_t dst_size;
}MpiArgs;

typedef struct IttArgs_ {
/*Add arguments here*/
}IttArgs;

typedef struct HostEventRecord_ {
  uint64_t id_;
  uint64_t start_time_;
  uint64_t end_time_;
  char name_[256];
  bool valid_name_ = false;
  API_TRACING_ID api_id_;
  EVENT_TYPE type_;

  API_TYPE api_type_ = API_TYPE::NONE;
  union{
    MpiArgs mpi_args_;
    IttArgs itt_args_;
   };
} HostEventRecord;



#endif // PTI_TOOLS_UNITRACE_UNIEVENT_H
