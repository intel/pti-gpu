//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef LEVEL_ZERO_ZE_EXTENSIONS_H_
#define LEVEL_ZERO_ZE_EXTENSIONS_H_

#include <level_zero/driver_experimental/zex_graph.h>
#include <level_zero/driver_experimental/zex_visit.h>
#include <level_zero/ze_intel_gpu.h>
#include <spdlog/spdlog.h>

#include "ze_utils.h"

struct ZeExts {
  template <typename Extension>
  [[nodiscard]] static bool IsSupported(ze_driver_handle_t driver) {
    return utils::ze::IsDriverExtensionSupported(driver, Extension::kExtName);
  }

  template <typename FnPtr>
  static bool LoadFunction(ze_driver_handle_t driver, std::string_view fn_name, FnPtr& fn_ptr) {
    auto* ptr = utils::ze::GetExtensionFunctionAddr(driver, fn_name.data());
    if (!ptr) {
      return false;
    }
    fn_ptr = reinterpret_cast<FnPtr>(ptr);
    return true;
  }

  struct Visit {
    static constexpr std::string_view kExtName = ZE_COMMAND_VISIT_EXT_NAME;

    decltype(&zeCommandListVisitExt) ze_command_list_visit = nullptr;
    decltype(&zeGraphVisitExt) ze_graph_visit = nullptr;

    [[nodiscard]] static std::optional<Visit> Load(ze_driver_handle_t driver) {
      if (!IsSupported<Visit>(driver)) {
        return std::nullopt;
      }

      Visit visit{};

      if (LoadFunction(driver, "zeCommandListVisitExt", visit.ze_command_list_visit) &&
          LoadFunction(driver, "zeGraphVisitExt", visit.ze_graph_visit)) {
        return visit;
      }

      return std::nullopt;
    }
  };

  struct CounterBasedEvents {
    static constexpr std::string_view kExtName = ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME;
    static std::optional<CounterBasedEvents> Load(ze_driver_handle_t driver) {
      if (!IsSupported<CounterBasedEvents>(driver)) {
        return std::nullopt;
      }
      return CounterBasedEvents{};
    }
  };

  struct CmdListIntrospection {
    static constexpr std::string_view kCmdListGetFlagsFunctionName = "zeCommandListGetFlags";
    decltype(&zeCommandListGetFlags) ze_command_list_get_flags = nullptr;
    static std::optional<CmdListIntrospection> Load(ze_driver_handle_t driver) {
      CmdListIntrospection cmd_list_introspection{};

      if (LoadFunction(driver, kCmdListGetFlagsFunctionName,
                       cmd_list_introspection.ze_command_list_get_flags)) {
        return cmd_list_introspection;
      }

      return std::nullopt;
    }
  };

  struct GraphExp {
    static constexpr std::string_view kExtName = ZE_RECORD_REPLAY_GRAPH_EXP_NAME;

    decltype(&zeGraphCreateExp) ze_graph_create_exp = nullptr;
    decltype(&zeCommandListBeginGraphCaptureExp) ze_command_list_begin_graph_capture_exp = nullptr;
    decltype(&zeCommandListBeginCaptureIntoGraphExp) ze_command_list_begin_capture_into_graph_exp =
        nullptr;
    decltype(&zeCommandListEndGraphCaptureExp) ze_command_list_end_graph_capture_exp = nullptr;
    decltype(&zeCommandListInstantiateGraphExp) ze_command_list_instantiate_graph_exp = nullptr;
    decltype(&zeCommandListAppendGraphExp) ze_command_list_append_graph_exp = nullptr;
    decltype(&zeGraphDestroyExp) ze_graph_destroy_exp = nullptr;
    decltype(&zeExecutableGraphDestroyExp) ze_executable_graph_destroy_exp = nullptr;
    decltype(&zeCommandListIsGraphCaptureEnabledExp) ze_command_list_is_graph_capture_enabled_exp =
        nullptr;
    decltype(&zeGraphIsEmptyExp) ze_graph_is_empty_exp = nullptr;
    decltype(&zeGraphDumpContentsExp) ze_graph_dump_contents_exp = nullptr;
    decltype(&zeCommandListGetGraphExp) ze_command_list_get_graph_exp = nullptr;
    decltype(&zeGraphSetDestructionCallbackExp) ze_graph_set_destruction_callback_exp = nullptr;
    decltype(&zeExecutableGraphGetSourceGraphExt) ze_executable_graph_get_source_graph_ext =
        nullptr;
    decltype(&zeGraphGetPrimaryCommandListExt) ze_graph_get_primary_command_list_ext = nullptr;

    static std::optional<GraphExp> Load(ze_driver_handle_t driver) {
      if (!IsSupported<GraphExp>(driver)) {
        return std::nullopt;
      }

      GraphExp graph{};

      if (LoadFunction(driver, "zeGraphCreateExp", graph.ze_graph_create_exp) &&
          LoadFunction(driver, "zeCommandListBeginGraphCaptureExp",
                       graph.ze_command_list_begin_graph_capture_exp) &&
          LoadFunction(driver, "zeCommandListBeginCaptureIntoGraphExp",
                       graph.ze_command_list_begin_capture_into_graph_exp) &&
          LoadFunction(driver, "zeCommandListEndGraphCaptureExp",
                       graph.ze_command_list_end_graph_capture_exp) &&
          LoadFunction(driver, "zeCommandListInstantiateGraphExp",
                       graph.ze_command_list_instantiate_graph_exp) &&
          LoadFunction(driver, "zeCommandListAppendGraphExp",
                       graph.ze_command_list_append_graph_exp) &&
          LoadFunction(driver, "zeGraphDestroyExp", graph.ze_graph_destroy_exp) &&
          LoadFunction(driver, "zeExecutableGraphDestroyExp",
                       graph.ze_executable_graph_destroy_exp) &&
          LoadFunction(driver, "zeCommandListIsGraphCaptureEnabledExp",
                       graph.ze_command_list_is_graph_capture_enabled_exp) &&
          LoadFunction(driver, "zeGraphIsEmptyExp", graph.ze_graph_is_empty_exp) &&
          LoadFunction(driver, "zeGraphDumpContentsExp", graph.ze_graph_dump_contents_exp) &&
          LoadFunction(driver, "zeCommandListGetGraphExp", graph.ze_command_list_get_graph_exp) &&
          LoadFunction(driver, "zeGraphSetDestructionCallbackExp",
                       graph.ze_graph_set_destruction_callback_exp) &&
          LoadFunction(driver, "zeExecutableGraphGetSourceGraphExt",
                       graph.ze_executable_graph_get_source_graph_ext) &&
          LoadFunction(driver, "zeGraphGetPrimaryCommandListExt",
                       graph.ze_graph_get_primary_command_list_ext)) {
        return graph;
      }

      return std::nullopt;
    }
  };

  using Extensions = std::tuple<std::optional<Visit>, std::optional<CounterBasedEvents>,
                                std::optional<CmdListIntrospection>, std::optional<GraphExp>>;
  Extensions extensions;
};

#endif  // LEVEL_ZERO_ZE_EXTENSIONS_H_
