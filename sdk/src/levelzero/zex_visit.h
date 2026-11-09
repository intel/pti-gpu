//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_LEVELZERO_ZEX_VISIT_H_
#define PTI_LEVELZERO_ZEX_VISIT_H_

#include <level_zero/ze_api.h>

#include <cstddef>
#include <cstdint>

// Release drivers reject the spec ZE_STRUCTURE_TYPE_*VISIT*_EXT_DESC values.
inline constexpr auto kZexStructureTypeConcreteVisitorExtDesc =
    static_cast<ze_structure_type_t>(0x00030031);
inline constexpr auto kZexStructureTypeCommandVisitExtDesc =
    static_cast<ze_structure_type_t>(0x00030032);

namespace zex {

// NOLINTBEGIN(readability-identifier-naming)
///////////////////////////////////////////////////////////////////////////////
/// @brief Defines default operation for visit call
typedef enum _ze_visit_ext_default_op_t {
  ZE_VISIT_EXT_DEFAULT_OP_IGNORE =
      0,  // if concrete visitor callback is not provided, ignore the command
  ZE_VISIT_EXT_DEFAULT_OP_REAPPEND =
      1,  // if concrete visitor callback is not provided, reappend the command
} ze_visit_ext_default_op_t;

// The driver's VISITOR_CCONV is __cdecl on Windows and empty elsewhere, same as ZE_VISITOR_CCONV.
typedef void(ZE_VISITOR_CCONV *ze_visit_ext_default_op_callback_t)(const char *fname,
                                                                   void *userData,
                                                                   uint32_t *numWaitEvents,
                                                                   ze_event_handle_t **phWaitEvents,
                                                                   ze_event_handle_t *hSignalEvent);
struct ze_concrete_visitor_ext_desc_t {
  uint32_t stype;  // ze_structure_type_ext_t
  const void *pNext;
  const char *fname;
  void *callback;
};

struct ze_visit_ext_desc_t {
  uint32_t stype;  // ze_structure_type_ext_t
  const void *pNext;
  void *userData;
  ze_command_list_handle_t hReappendTargetCmdList;
  ze_visit_ext_default_op_callback_t beforeDefaultOpClb;
  ze_visit_ext_default_op_t defaultOp;
  ze_visit_ext_default_op_callback_t afterDefaultOpClb;
};
// NOLINTEND(readability-identifier-naming)
}  // namespace zex

static_assert(sizeof(zex::ze_concrete_visitor_ext_desc_t) ==
              sizeof(ze_concrete_visitor_ext_desc_t));
static_assert(alignof(zex::ze_concrete_visitor_ext_desc_t) ==
              alignof(ze_concrete_visitor_ext_desc_t));
static_assert(offsetof(zex::ze_concrete_visitor_ext_desc_t, pNext) ==
              offsetof(ze_concrete_visitor_ext_desc_t, pNext));
static_assert(offsetof(zex::ze_concrete_visitor_ext_desc_t, fname) ==
              offsetof(ze_concrete_visitor_ext_desc_t, fname));
static_assert(offsetof(zex::ze_concrete_visitor_ext_desc_t, callback) ==
              offsetof(ze_concrete_visitor_ext_desc_t, callback));

static_assert(sizeof(zex::ze_visit_ext_desc_t) == sizeof(ze_command_visit_ext_desc_t));
static_assert(alignof(zex::ze_visit_ext_desc_t) == alignof(ze_command_visit_ext_desc_t));
static_assert(offsetof(zex::ze_visit_ext_desc_t, pNext) ==
              offsetof(ze_command_visit_ext_desc_t, pNext));
static_assert(offsetof(zex::ze_visit_ext_desc_t, userData) ==
              offsetof(ze_command_visit_ext_desc_t, userData));
static_assert(offsetof(zex::ze_visit_ext_desc_t, hReappendTargetCmdList) ==
              offsetof(ze_command_visit_ext_desc_t, hReappendTargetCommandList));
static_assert(offsetof(zex::ze_visit_ext_desc_t, beforeDefaultOpClb) ==
              offsetof(ze_command_visit_ext_desc_t, beforeDefaultOpClb));
static_assert(offsetof(zex::ze_visit_ext_desc_t, defaultOp) ==
              offsetof(ze_command_visit_ext_desc_t, defaultOp));
static_assert(offsetof(zex::ze_visit_ext_desc_t, afterDefaultOpClb) ==
              offsetof(ze_command_visit_ext_desc_t, afterDefaultOpClb));

static_assert(static_cast<int>(zex::ZE_VISIT_EXT_DEFAULT_OP_REAPPEND) ==
              static_cast<int>(ZE_COMMAND_VISIT_EXT_DEFAULT_OP_REAPPEND));

// The driver's ZE_COMMAND_LIST_FLAG_ENABLE_CMD_VISITING.
static_assert(ZE_COMMAND_LIST_FLAG_ENABLE_COMMAND_VISIT == ZE_BIT(6));

#endif  // PTI_LEVELZERO_ZEX_VISIT_H_
