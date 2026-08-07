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

  struct GraphExt {
    static constexpr std::string_view kExtName = ZE_RECORD_REPLAY_GRAPH_EXT_NAME;

    static std::optional<GraphExt> Load(ze_driver_handle_t driver) {
      if (!IsSupported<GraphExt>(driver)) {
        return std::nullopt;
      }
      return GraphExt{};
    }
  };

  using Extensions = std::tuple<std::optional<Visit>, std::optional<CounterBasedEvents>,
                                std::optional<CmdListIntrospection>, std::optional<GraphExt>>;
  Extensions extensions;
};

#endif  // LEVEL_ZERO_ZE_EXTENSIONS_H_
