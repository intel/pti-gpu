//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_METRICS_CONFIG_H_
#define PTI_TOOLS_UNITRACE_METRICS_CONFIG_H_

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <level_zero/zet_api.h>
#include "ze_loader.h"

#include <string>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>
#include <string>
#include <vector>
#include <unordered_map>

#include "utils/unitrace_assert.h"
#include "config_node.h"
#include "metric_config_validator.h"


// Formats a sorted list of valid parameter values compactly for the "Valid values"
// comments of a generated config template. Each run of three or more values with a
// common step is printed as "first:last" (step 1) or "first:last:step"; shorter
// runs are printed as individual values. E.g. 0,1,2,3,8,10,12 -> "0:3,8:12:2".
template <class T>
class RangeStringify {
public:
  RangeStringify() = default;
  void add(T value) {
    if (values_.size() < 2 || (value - values_.back() == (values_[1] - values_[0]))) {
      values_.push_back(value);
      return;
    }
    flush();
    values_.push_back(value);
  }

  std::string str() {
   flush();
   return str_;
  }

private:
  void flush() {
    if (values_.size() == 0) {
      return;
    }
    if (str_.size() > 0) {
      str_ += ",";
    }
    if (values_.size() > 2) {
      str_ += ToString(values_[0]) + ":" + ToString(values_.back());
      if (values_[1] - values_[0] != (T)1) {
        str_ += ":" + ToString(values_[1] - values_[0]);
      }
    } else {
      for (auto &val: values_) {
        if (&val != &values_[0]) str_ += ",";
        str_ += ToString(val);
      }
    }
    values_.clear();
  }

  static std::string ToString(T value) {
    if constexpr (std::is_floating_point_v<T>) {
      return config_node_detail::FormatDouble(value);
    } else {
      return std::to_string(value);
    }
  }

  std::vector<T> values_;  // current run, flushed when the step changes
  std::string str_;
};

// Builds the single metric group unitrace activates on a device, from the -g
// group name(s) and/or the --metric-config file, for one sampling type
// (event-based for -q, time-based for -k/--stall-sampling).
//
// Two kinds of groups can be selected:
//  - predefined groups, enumerated and owned by the driver;
//  - programmable metrics: each enabled "programmable" entry creates one or more
//    metrics from a driver programmable with the configured parameter values;
//    all created metrics are then combined into new metric groups. The created
//    metrics and groups are owned by this object and destroyed with it, so it
//    must outlive any activation or calculation that uses its groups.
//
// It also generates a config template listing everything the device supports
// (GenerateAllMetricConfig).
class ZeMetricsManager {
 public:
  ZeMetricsManager(ze_device_handle_t device, zet_metric_group_sampling_type_flag_t samplingType) :
                    m_device_(device),
                    m_samplingType_(samplingType),
                    m_prog_groups_start_(0) {
    InitPredefined();
    InitProgrammables();
  }

  // Owns driver handles that are destroyed exactly once in the destructor.
  ZeMetricsManager(const ZeMetricsManager&) = delete;
  ZeMetricsManager& operator=(const ZeMetricsManager&) = delete;

  // Selects the groups in groups_list (comma separated predefined names, may be
  // empty) plus the enabled groups and programmables of the metric_config file
  // (may be empty). Exactly one group must result: returns nullptr (after
  // printing the reason) if none or more than one does, or on any config error.
  // On success the caller owns the returned object and must delete it after the
  // group is no longer used.
  static ZeMetricsManager* CreateMetricGroups(ze_device_handle_t device, zet_metric_group_sampling_type_flag_t samplingType, const std::string &metric_config, const std::string &groups_list) {
    ConfigNodePtr conf;
    if (!metric_config.empty()) {
      std::string error;
      conf = metric_config::Load(metric_config, metric_config::CountGroups(groups_list), error);
      if (conf == nullptr) {
        std::cerr << "[ERROR] Invalid metric configuration file: " << error << std::endl;
        return nullptr;
      }
    }

    ZeMetricsManager* metricsManager = new ZeMetricsManager(device, samplingType);

    // Add predefined metric groups based on comma separated groups_list if it's not empty.
    if (!groups_list.empty()) {
      std::istringstream ss(groups_list);
      std::string group_name;
      while (std::getline(ss, group_name, ',')) {
        if (!metricsManager->addPredefinedMetricGroup(group_name)) {
          delete metricsManager;
          return nullptr;
        }
      }
    }

    // Add groups and metrics from the (already validated) config file if provided.
    if (conf) {
      // Predefined metric groups
      auto cpredef = conf->findChildByName("predefined");
      if (cpredef != nullptr) {
        for (auto cgroup : cpredef->getChildren()) {
          if (!IsNodeEnabled(cgroup)) {
            continue;
          }

          if (!metricsManager->addPredefinedMetricGroup(cgroup->getName())) {
            delete metricsManager;
            return nullptr;
          }
        }
      }

      // Programmable metric groups
      auto cprog = conf->findChildByName("programmable");
      if (cprog != nullptr) {
        for (auto cmetric : cprog->getChildren()) {
          if (!metricsManager->createProgramMetrics(cmetric)) {
            delete metricsManager;
            return nullptr;
          }
        }
        metricsManager->createProgMetricGroups();
      }
    }

    if (metricsManager->getMetricGroupsCount() == 0) {
      std::cerr << "[ERROR] No valid metric groups found based on the provided configuration." << std::endl;
      delete metricsManager;
      return nullptr;
    } else if (metricsManager->getMetricGroupsCount() > 1) {
      std::cerr << "[ERROR] Only one metric group can be collected, but " << metricsManager->getMetricGroupsCount()
                << " were selected on this device: " << metricsManager->DescribeMetricGroups()
                << ". Enable a single predefined group, or programmable metrics only (without -g)." << std::endl;
      delete metricsManager;
      return nullptr;
    }

    return metricsManager;
  }

  static bool GenerateAllMetricConfig(ze_device_handle_t device, const std::string &metric_config) {
    ConfigNodePtr conf = std::make_shared<ConfigNode>("");
    // Predefined metric groups
    ConfigNodePtr cpredef = std::make_shared<ConfigNode>("predefined");
    conf->addChild(cpredef);
    if (!fillAllPredefinedMetricGroup(device, cpredef)) {
      return false;
    }

    // Programmable metric groups
    ConfigNodePtr cprog = std::make_shared<ConfigNode>("programmable");
    conf->addChild(cprog);
    if (!fillAllProgrammables(device, cprog)) {
      return false;
    }

    return ConfigNode::save(conf, metric_config);
  }

  const zet_metric_group_handle_t* GetMetricGroups() {
    if( m_metric_groups_.size() == 0) {
      return nullptr;
    }
    return m_metric_groups_.data();
  }

  // Builds, for each parameter, the "Valid values" text written to the template:
  //  - enumerated integer/float values: compressed by RangeStringify, e.g. "0:19";
  //  - UINT64_RANGE / FLOAT64_RANGE: the reported intervals as "min:max,...";
  //  - BOOL8: the reported values, or "false,true" if none are reported.
  static void buildParamValuesRangeDesc(zet_metric_programmable_exp_handle_t prog,
                                        const std::vector<zet_metric_programmable_param_info_exp_t> &params,
                                        std::vector<std::string> &paramsValuesRangeDesc) {

    auto to_ui64 = [](zet_value_info_exp_t &vinfo, zet_value_info_type_exp_t vtype) {
      switch(vtype) {
      case ZET_VALUE_INFO_TYPE_EXP_UINT32:
        return (uint64_t)vinfo.ui32;
      case ZET_VALUE_INFO_TYPE_EXP_UINT64:
        return (uint64_t)vinfo.ui64;
      case ZET_VALUE_INFO_TYPE_EXP_UINT8:
        return (uint64_t)vinfo.ui8;
      case ZET_VALUE_INFO_TYPE_EXP_UINT16:
        return (uint64_t)vinfo.ui16;
      default:
        UNITRACE_ASSERT(false && "Invalid value type for uint64 conversion");
      }
      return UINT64_MAX;
    };

    auto to_double = [](zet_value_info_exp_t &vinfo, zet_value_info_type_exp_t vtype) {
      switch(vtype) {
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
        return (double)vinfo.fp32;
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
        return (double)vinfo.fp64;
      default:
        UNITRACE_ASSERT(false && "Invalid value type for float64 conversion");
      }
      return std::numeric_limits<double>::max();
    };

    for (uint32_t i = 0; i < params.size(); i++) {
      uint32_t val_num = params[i].valueInfoCount;
      std::vector<zet_metric_programmable_param_value_info_exp_t> val_info(val_num);
      ze_result_t status = ZE_FUNC(zetMetricProgrammableGetParamValueInfoExp)(prog, i, &val_num, val_info.data());
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

      std::string str;

      if (params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE) {
        for (uint32_t j = 0; j < val_num; j++) {
          str += std::to_string(val_info[j].valueInfo.ui64Range.ui64Min) + ":" +
                 std::to_string(val_info[j].valueInfo.ui64Range.ui64Max);
            if (j != val_num - 1) {
                str += ",";
            }
        }
      } else if (params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE) {
        for (uint32_t j = 0; j < val_num; j++) {
          str += config_node_detail::FormatDouble(val_info[j].valueInfo.fp64Range.fp64Min) + ":" +
                 config_node_detail::FormatDouble(val_info[j].valueInfo.fp64Range.fp64Max);
            if (j != val_num - 1) {
                str += ",";
            }
        }
      } else if (params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_UINT32 ||
               params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_UINT64 ||
               params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_UINT8 ||
               params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_UINT16) {
        RangeStringify<uint64_t> range_ui64;
        for (uint32_t j = 0; j < val_num; j++) {
          range_ui64.add(to_ui64(val_info[j].valueInfo, params[i].valueInfoType));
        }
        str = range_ui64.str();
      } else if (params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_FLOAT32 ||
               params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_FLOAT64) {
        RangeStringify<double> range_fp64;
        for (uint32_t j = 0; j < val_num; j++) {
          range_fp64.add(to_double(val_info[j].valueInfo, params[i].valueInfoType));
        }
        str = range_fp64.str();
      } else if (params[i].valueInfoType == ZET_VALUE_INFO_TYPE_EXP_BOOL8) {
        if (val_num > 0) {
          for (uint32_t j = 0; j < val_num; j++) {
              str += (val_info[j].valueInfo.b8 != 0) ? "true" : "false";
              if (j != val_num - 1) {
                  str += ",";
              }
          }
        } else {
            str = "false,true";
        }
      } else {
          UNITRACE_ASSERT(false && "Unsupported value info type for range description");
      }

      paramsValuesRangeDesc.push_back(str);
    }
  }

  // Adds a disabled entry per predefined group to the template. The driver reports
  // a group once per sampling type, so groups are merged by name and the comment
  // lists all sampling types the name supports.
  static bool fillAllPredefinedMetricGroup(ze_device_handle_t device, ConfigNodePtr cpredef_metric) {
    uint32_t num_groups = 0;
    ze_result_t status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, nullptr);
    UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
    if (num_groups > 0) {
      std::vector<zet_metric_group_handle_t> groups(num_groups, nullptr);
      status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, groups.data());
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

      std::vector<std::pair<std::string, zet_metric_group_sampling_type_flags_t>> groups_vec;
      std::unordered_map<std::string, uint32_t> group_idx;

      for (uint32_t i = 0; i < num_groups; ++i) {
        zet_metric_group_properties_t group_props{};
        group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        status = ZE_FUNC(zetMetricGroupGetProperties)(groups[i], &group_props);
        if (status != ZE_RESULT_SUCCESS) {
          UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
          return false;
        }

        auto it = group_idx.find(group_props.name);
        if (it != group_idx.end()) {
          groups_vec[it->second].second |= group_props.samplingType;
        } else {
          group_idx[group_props.name] = groups_vec.size();
          groups_vec.emplace_back(group_props.name, group_props.samplingType);
        }
      }

      for (auto &[name, samplingTypes] : groups_vec) {
        if ((samplingTypes & kCollectableSamplingTypes) == 0) {
          continue;
        }
        std::string desc = getSupportedSamplingTypesDesc(samplingTypes);

        ConfigNodePtr groupNode = std::make_shared<ConfigNode>(name);
        groupNode->addAttribute(std::make_shared<BoolAttribute>("enable", false, desc));
        cpredef_metric->addChild(groupNode);
      }
    }
    return true;
  }

  // A programmable as it appears in the config file. Programmables sharing a name
  // but differing in their parameter set get the key "<name>_<params hash>"; the
  // first one keeps the plain name. Programmables with the same name and parameter
  // set (e.g. one per sampling type) share one key.
  struct ProgrammableEntry {
    std::string key;
    std::string name;
    zet_metric_programmable_exp_handle_t handle;
    zet_metric_group_sampling_type_flags_t samplingType;
    uint32_t parameterCount;
    std::string params_signature;  // full description of the parameter set, see ParamsSignature
    std::size_t params_hash;       // hash of params_signature, used in "<name>_<hash>" keys
    std::vector<zet_metric_programmable_param_info_exp_t> params;
    std::vector<std::string> paramsValuesRangeDesc;
  };

  // Enumerates the programmables of a device and assigns their config keys.
  // Parameter details are fetched for every programmable when with_params is true,
  // otherwise only for names that are shared, where they are needed for the key.
  // The hash is not stable across builds, so a config file using "<name>_<hash>"
  // keys must be generated by the same unitrace build.
  static std::vector<ProgrammableEntry> EnumerateProgrammables(ze_device_handle_t device, bool with_params) {
    std::vector<ProgrammableEntry> entries;

    uint32_t num_progs = 0;
    ze_result_t status = ZE_FUNC(zetMetricProgrammableGetExp)(device, &num_progs, nullptr);
    if (status != ZE_RESULT_SUCCESS || num_progs == 0) {
      return entries;
    }
    std::vector<zet_metric_programmable_exp_handle_t> progs(num_progs, nullptr);
    status = ZE_FUNC(zetMetricProgrammableGetExp)(device, &num_progs, progs.data());
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to get programmable metrics (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
      return entries;
    }

    std::vector<std::string> names;
    std::unordered_map<std::string, uint32_t> name_count;
    for (uint32_t i = 0; i < num_progs; ++i) {
      zet_metric_programmable_exp_properties_t prog_props{};
      prog_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROGRAMMABLE_EXP_PROPERTIES;
      status = ZE_FUNC(zetMetricProgrammableGetPropertiesExp)(progs[i], &prog_props);
      if (status != ZE_RESULT_SUCCESS) {
        std::cerr << "[WARNING] Unable to get properties of programmable metric #" << i << " (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
        continue;
      }
      ProgrammableEntry entry{};
      entry.key = prog_props.name;
      entry.name = prog_props.name;
      entry.handle = progs[i];
      entry.samplingType = prog_props.samplingType;
      entry.parameterCount = prog_props.parameterCount;
      ++name_count[entry.key];
      entries.push_back(std::move(entry));
    }

    std::vector<ProgrammableEntry> result;
    std::unordered_map<std::string, std::string> key_signature;  // assigned key -> params signature
    for (auto& entry : entries) {
      const std::string& name = entry.name;
      const bool shared_name = name_count[name] > 1;
      if (with_params || shared_name) {
        uint32_t num_params = entry.parameterCount;
        entry.params.resize(num_params);
        status = ZE_FUNC(zetMetricProgrammableGetParamInfoExp)(entry.handle, &num_params, entry.params.data());
        if (status != ZE_RESULT_SUCCESS) {
          std::cerr << "[WARNING] Unable to get parameters of programmable metric " << name << " (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
          continue;
        }
        entry.params.resize(num_params);
        buildParamValuesRangeDesc(entry.handle, entry.params, entry.paramsValuesRangeDesc);
        entry.params_signature = ParamsSignature(entry.params, entry.paramsValuesRangeDesc);
        entry.params_hash = std::hash<std::string>{}(entry.params_signature);
      }

      if (shared_name) {
        auto it = key_signature.find(name);
        if (it != key_signature.end() && it->second != entry.params_signature) {
          entry.key = name + "_" + std::to_string(entry.params_hash);
        }
      }
      key_signature.insert({entry.key, entry.params_signature});
      result.push_back(std::move(entry));
    }
    return result;
  }

  static bool fillAllProgrammables(ze_device_handle_t device, ConfigNodePtr cprog_metric) {
    //
    // Programmables sharing a key (same name and parameter set) are represented by
    // one config entry with combined supported sampling types.
    //
    struct ProgInfo {
      std::string name;
      zet_metric_group_sampling_type_flags_t samplingTypes;
      std::string params_signature;
    };

    std::vector<ProgInfo> progs_vec;
    std::unordered_map<std::string, uint32_t> prog_idx;
    // Programmables with identical parameter sets (names, types, defaults, valid values) share one node.
    std::map<std::string, ConfigNodePtr> param_nodes;

    for (const auto& entry : EnumerateProgrammables(device, true)) {
      auto it = prog_idx.find(entry.key);
      if (it != prog_idx.end()) {
        progs_vec[it->second].samplingTypes |= entry.samplingType;
        continue;
      }

      if (param_nodes.find(entry.params_signature) == param_nodes.end()) {
        ConfigNodePtr param_node = std::make_shared<ConfigNode>("parameters");
        for (size_t j = 0; j < entry.params.size(); ++j) {
          std::string desc = !entry.paramsValuesRangeDesc[j].empty() ? "Valid values: " + entry.paramsValuesRangeDesc[j] : "";
          addParam(entry.params[j], param_node, desc);
        }
        param_nodes[entry.params_signature] = param_node;
      }

      prog_idx[entry.key] = progs_vec.size();
      progs_vec.push_back({entry.key, entry.samplingType, entry.params_signature});
    }

    for (auto &pinfo : progs_vec) {
      if ((pinfo.samplingTypes & kCollectableSamplingTypes) == 0) {
        continue;
      }
      std::string desc = getSupportedSamplingTypesDesc(pinfo.samplingTypes);

      ConfigNodePtr progNode = std::make_shared<ConfigNode>(pinfo.name);
      progNode->addAttribute(std::make_shared<BoolAttribute>("enable", false, desc));
      progNode->addChild(param_nodes[pinfo.params_signature]);
      cprog_metric->addChild(progNode);
    }

    return true;
  }

  std::size_t getMetricGroupsCount() {
    return m_metric_groups_.size();
  }

  // Comma-separated names of the selected groups; groups created from
  // programmable metrics are named by the driver.
  std::string DescribeMetricGroups() const {
    std::string names;
    for (auto group : m_metric_groups_) {
      zet_metric_group_properties_t props{};
      props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
      if (!names.empty()) {
        names += ", ";
      }
      names += (ZE_FUNC(zetMetricGroupGetProperties)(group, &props) == ZE_RESULT_SUCCESS) ? props.name : "<unknown>";
    }
    return names;
  }

  // Created groups must be destroyed before the metrics they contain.
  ~ZeMetricsManager() {
    for (uint32_t i = m_prog_groups_start_; i < m_metric_groups_.size(); ++i) {
      ze_result_t status = ZE_FUNC(zetMetricGroupDestroyExp)(m_metric_groups_[i]);
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    for (auto metric : m_all_progs_metrics_) {
      ze_result_t status = ZE_FUNC(zetMetricDestroyExp)(metric);
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

 protected:
  void InitPredefined() {
    uint32_t num_groups = 0;
    ze_result_t status = ZE_FUNC(zetMetricGroupGet)(m_device_, &num_groups, nullptr);
    UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
    if (num_groups > 0) {
      std::vector<zet_metric_group_handle_t> groups(num_groups, nullptr);
      status = ZE_FUNC(zetMetricGroupGet)(m_device_, &num_groups, groups.data());
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

      for (uint32_t i = 0; i < num_groups; ++i) {
        zet_metric_group_properties_t group_props{};
        group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        status = ZE_FUNC(zetMetricGroupGetProperties)(groups[i], &group_props);
        UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

        if ((group_props.samplingType & m_samplingType_) != 0) {
          m_predef_groups_.insert({std::string(group_props.name), groups[i]});
        }
      }
    }
  }

  void InitProgrammables() {
    for (const auto& entry : EnumerateProgrammables(m_device_, false)) {
      if ((entry.samplingType & m_samplingType_) != 0) {
        m_programmables_.insert({entry.key, {entry.handle, entry.parameterCount, entry.name}});
      }
    }
  }

  // Unknown names (or names without the requested sampling type) only warn, so one
  // config can be shared by devices and sampling modes; an empty result is
  // reported by CreateMetricGroups.
  bool addPredefinedMetricGroup(const std::string &name) {
    if(name.empty()) {
      std::cerr << "[ERROR] Predefined metric without name." << std::endl;
      return false;
     }
    auto mgroup = m_predef_groups_.find(name);
    if (mgroup == m_predef_groups_.end()) {
      std::cerr << "[WARNING] Predefined metric(" << name << ") not found." << std::endl;
      return true;
    }

    // Keep predefined groups before the created ones (see m_prog_groups_start_).
    m_metric_groups_.insert(std::next(m_metric_groups_.begin(), m_prog_groups_start_), mgroup->second);
    ++m_prog_groups_start_;
    return true;
  }

  // Creates the metrics of one "programmable" config entry and appends them to
  // m_all_progs_metrics_:
  //  1. start from the driver default of every parameter;
  //  2. override the parameters listed under "parameters", validating each value
  //     against the values the driver reports as valid;
  //  3. at most one parameter may be a range list ([0,2,4:8]); the programmable is
  //     instantiated once per value of that list, otherwise once.
  // One instantiation can yield several metrics (e.g. per disaggregation level).
  // Disabled or unknown entries are skipped; returns false on config errors.
  bool createProgramMetrics(ConfigNodePtr cprogram_metric) {
    auto cname = cprogram_metric->getName();
    if (cname.empty()) {
      std::cerr << "[ERROR] Programmable without \"name\"." << std::endl;
      return false;
    }

    if (!IsNodeEnabled(cprogram_metric)) {
      return true;
    }

    auto prog = m_programmables_.find(cname);
    if (prog == m_programmables_.end()) {
      std::cerr << "[WARNING] Programmable(" << cname << ") not found." << std::endl;
      return true;
    }

    // The config key may carry a "_<hash>" suffix; created metrics use the programmable's own name.
    const char* prog_name = prog->second.name.c_str();
    zet_metric_programmable_exp_handle_t programmable = prog->second.handle;
    uint32_t num_params = prog->second.parameterCount;

    std::vector<zet_metric_programmable_param_info_exp_t> params(num_params);
    std::vector<zet_metric_programmable_param_value_exp_t> vals(num_params);
    ze_result_t status = ZE_FUNC(zetMetricProgrammableGetParamInfoExp)(programmable, &num_params, params.data());
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[ERROR] Unable to get parameters of programmable(" << cname << ") (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
      return false;
    }
    params.resize(num_params);
    vals.resize(num_params);

    FillDefaultVals(params, vals);

    struct RangeParamIterator {
      uint32_t index;
      const U64RangeListAttribute *range_attr;
      uint64_t current;
    };
    std::vector<RangeParamIterator> range_params;

    auto cparams = cprogram_metric->findChildByName("parameters");
    if (cparams != nullptr) {
      for (const auto& aparam : cparams->getAttributes()) {
        auto apname = aparam->getKey();
        if (apname.empty()) {
          std::cerr << "[ERROR] Parameter of programmable(" << cname << ") without \"name\"." << std::endl;
          return false;
        }

        uint32_t i;
        for (i = 0; i < num_params; ++i) {
          if (apname.compare(params[i].name) != 0) {
            continue;
          }

          uint32_t val_num = params[i].valueInfoCount;
          std::vector<zet_metric_programmable_param_value_info_exp_t> val_info(val_num);
          status = ZE_FUNC(zetMetricProgrammableGetParamValueInfoExp)(programmable, i, &val_num, val_info.data());
          if (status != ZE_RESULT_SUCCESS) {
            std::cerr << "[ERROR] Unable to get valid values of Parameter(" << apname << ") of programmable(" << cname << ")" << std::endl;
            return false;
          }
          val_info.resize(val_num);

          if (aparam->getType() == AttributeType::UNSIGNED_64_RANGE_LIST) {
            if (!IsUintValueType(params[i].valueInfoType)) {
              std::cerr << "[ERROR] Parameter(" << apname << ") of programmable(" << cname << ") cannot be set to uint values." << std::endl;
              return false;
            }
            if (!range_params.empty()) {
              std::cerr << "[ERROR] Multiple range parameters for programmable(" << cname << ") are not supported yet." << std::endl;
              return false;
            }
            const U64RangeListAttribute *range_attr = dynamic_cast<const U64RangeListAttribute*>(aparam.get());
            if (range_attr == nullptr) {
              std::cerr << "[ERROR] Parameter(" << apname << ") of programmable(" << cname << ") has invalid value type." << std::endl;
              return false;
            }

            // Validate every value up front so that an invalid value is reported, not passed to the driver.
            uint64_t value = range_attr->getMin();
            uint32_t value_count = 0;
            do {
              if (++value_count > max_range_values) {
                std::cerr << "[ERROR] Parameter(" << apname << ") of programmable(" << cname << ") has more than " << max_range_values << " values." << std::endl;
                return false;
              }
              if (!IsUintValueValid(params[i].valueInfoType, val_info, value)) {
                std::cerr << "[ERROR] Value " << value << " of Parameter(" << apname << ") of programmable(" << cname << ") is not valid." << std::endl;
                return false;
              }
            } while (range_attr->getNext(value, value));

            SetUintValue(params[i].valueInfoType, range_attr->getMin(), vals[i]);
            range_params.push_back({i, range_attr, range_attr->getMin()});
            break;
          }

          std::string reason = SetParamValue(aparam, params[i], val_info, vals[i]);
          if (!reason.empty()) {
            std::cerr << "[ERROR] Parameter(" << apname << ") of programmable(" << cname << "): " << reason << std::endl;
            return false;
          }
          break;
        }
        if (i == num_params) {
          std::cerr << "[ERROR] Parameter(" << apname << ") of programmable(" << cname << ") not found." << std::endl;
          return false;
        }
      }
    }

    // Create metrics for every value of the (at most one) range parameter.
    while (true) {
      uint32_t num_metrics = 0;
      status = ZE_FUNC(zetMetricCreateFromProgrammableExp2)(programmable, num_params, vals.data(), prog_name, "prog description", &num_metrics, nullptr);
      if (status != ZE_RESULT_SUCCESS) {
        std::cerr << "[ERROR] Unable to create metrics from programmable(" << cname << ") (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
        return false;
      }
      if (num_metrics == 0) {
        std::cerr << "[WARNING] No metric was created for programmable(" << cname << ")." << std::endl;
        return false;
      }
      size_t offset = m_all_progs_metrics_.size();
      m_all_progs_metrics_.resize(offset + num_metrics);
      status = ZE_FUNC(zetMetricCreateFromProgrammableExp2)(programmable, num_params, vals.data(), prog_name, "prog description", &num_metrics, m_all_progs_metrics_.data() + offset);
      if (status != ZE_RESULT_SUCCESS) {
        m_all_progs_metrics_.resize(offset);
        std::cerr << "[ERROR] Unable to create metrics from programmable(" << cname << ") (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
        return false;
      }
      m_all_progs_metrics_.resize(offset + num_metrics);

      if (range_params.empty()) {
        break;
      }
      uint64_t next = 0;
      if (!range_params[0].range_attr->getNext(range_params[0].current, next)) {
        break;
      }
      range_params[0].current = next;
      SetUintValue(params[range_params[0].index].valueInfoType, next, vals[range_params[0].index]);
    }

    return true;
  }

  // Combines all created metrics into metric groups. The driver puts metrics whose
  // hardware resources do not overlap into the same group, so conflicting metrics
  // yield several groups, which CreateMetricGroups then rejects.
  void createProgMetricGroups() {
    std::vector<zet_metric_group_handle_t> groups_to_create;
    if (m_all_progs_metrics_.size() == 0) {
      return;
    }
    uint32_t num_metric_groups = 0;
    ze_result_t status = ZE_FUNC(zetDeviceCreateMetricGroupsFromMetricsExp)(m_device_, m_all_progs_metrics_.size(), m_all_progs_metrics_.data(), "Progs_", "_Disposable", &num_metric_groups, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[ERROR] Unable to create metric groups from programmable metrics (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
      return;
    }
    if (num_metric_groups != 0) {
      groups_to_create.resize(num_metric_groups);
      status = ZE_FUNC(zetDeviceCreateMetricGroupsFromMetricsExp)(m_device_, m_all_progs_metrics_.size(), m_all_progs_metrics_.data(), "Progs_", "_Disposable", &num_metric_groups, groups_to_create.data());
      if (status != ZE_RESULT_SUCCESS) {
        std::cerr << "[ERROR] Unable to create metric groups from programmable metrics (status = 0x" << std::hex << status << std::dec << ")" << std::endl;
        return;
      }
      // Actual number of groups may be less than what previously reported for allocation purposes.
      groups_to_create.resize(num_metric_groups);
    }

    for (auto group : groups_to_create) {
      zet_metric_group_properties_t metric_group_props{};
      metric_group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
      status = ZE_FUNC(zetMetricGroupGetProperties)(group, &metric_group_props);
      // A group may support more than one sampling type; keep it if it supports the requested one.
      if (status == ZE_RESULT_SUCCESS && (metric_group_props.samplingType & m_samplingType_) != 0) {
        m_metric_groups_.push_back(group);
      } else {
        status = ZE_FUNC(zetMetricGroupDestroyExp)(group);
        UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
      }
    }

  }

 private:
  // Upper bound on the values of a range parameter, i.e. on metrics created per programmable.
  static constexpr uint32_t max_range_values = 4096;

  // Default value of a parameter as config text. The default is a zet_value_t,
  // which has no 8/16-bit members: those defaults are held in ui32.
  static std::string DefaultValueString(const zet_metric_programmable_param_info_exp_t &param) {
    const auto &v = param.defaultValue;
    switch (param.valueInfoType) {
      case ZET_VALUE_INFO_TYPE_EXP_UINT8:
      case ZET_VALUE_INFO_TYPE_EXP_UINT16:
      case ZET_VALUE_INFO_TYPE_EXP_UINT32:        return std::to_string(v.ui32);
      case ZET_VALUE_INFO_TYPE_EXP_UINT64:
      case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:  return std::to_string(v.ui64);
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:       return config_node_detail::FormatDouble(v.fp32);
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE: return config_node_detail::FormatDouble(v.fp64);
      case ZET_VALUE_INFO_TYPE_EXP_BOOL8:         return v.b8 ? "true" : "false";
      default:                                    return "?";
    }
  }

  // Everything that makes up the "parameters" node of a template entry: name, type,
  // default value and valid values of every parameter. Two programmables can share
  // a node, or a config key, only if their signatures are equal.
  static std::string ParamsSignature(const std::vector<zet_metric_programmable_param_info_exp_t> &params,
                                     const std::vector<std::string> &descs) {
    std::string sig;
    for (size_t i = 0; i < params.size(); ++i) {
      sig += std::string(params[i].name) + '\x1f' + std::to_string(params[i].valueInfoType) + '\x1f' +
             std::to_string(params[i].valueInfoCount) + '\x1f' + DefaultValueString(params[i]) + '\x1f' +
             (i < descs.size() ? descs[i] : std::string()) + '\x1e';
    }
    return sig;
  }

  static bool IsFloatValueType(zet_value_info_type_exp_t type) {
    return type == ZET_VALUE_INFO_TYPE_EXP_FLOAT32 || type == ZET_VALUE_INFO_TYPE_EXP_FLOAT64 ||
           type == ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE;
  }

  // An empty val_info means the driver did not report valid values; leave validation to the driver.
  static bool IsFloatValueValid(zet_value_info_type_exp_t type,
                                const std::vector<zet_metric_programmable_param_value_info_exp_t> &val_info,
                                double value) {
    if (val_info.empty()) {
      return true;
    }
    for (const auto &vi : val_info) {
      switch (type) {
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
          // compare in the parameter's precision: 0.1 in the file is 0.1f for a FLOAT32 parameter
          if (vi.valueInfo.fp32 == static_cast<float>(value)) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
          if (vi.valueInfo.fp64 == value) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE:
          if (vi.valueInfo.fp64Range.fp64Min <= value && value <= vi.valueInfo.fp64Range.fp64Max) return true;
          break;
        default:
          return false;
      }
    }
    return false;
  }

  static bool IsUintValueType(zet_value_info_type_exp_t type) {
    return type == ZET_VALUE_INFO_TYPE_EXP_UINT8 || type == ZET_VALUE_INFO_TYPE_EXP_UINT16 ||
           type == ZET_VALUE_INFO_TYPE_EXP_UINT32 || type == ZET_VALUE_INFO_TYPE_EXP_UINT64 ||
           type == ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE;
  }

  // An empty val_info means the driver did not report valid values; leave validation to the driver.
  static bool IsUintValueValid(zet_value_info_type_exp_t type,
                               const std::vector<zet_metric_programmable_param_value_info_exp_t> &val_info,
                               uint64_t value) {
    if ((type == ZET_VALUE_INFO_TYPE_EXP_UINT8 && value > UINT8_MAX) ||
        (type == ZET_VALUE_INFO_TYPE_EXP_UINT16 && value > UINT16_MAX) ||
        (type == ZET_VALUE_INFO_TYPE_EXP_UINT32 && value > UINT32_MAX)) {
      return false;
    }
    if (val_info.empty()) {
      return true;
    }
    for (const auto &vi : val_info) {
      switch (type) {
        case ZET_VALUE_INFO_TYPE_EXP_UINT8:
          if (vi.valueInfo.ui8 == value) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT16:
          if (vi.valueInfo.ui16 == value) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT32:
          if (vi.valueInfo.ui32 == value) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64:
          if (vi.valueInfo.ui64 == value) return true;
          break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
          if (vi.valueInfo.ui64Range.ui64Min <= value && value <= vi.valueInfo.ui64Range.ui64Max) return true;
          break;
        default:
          return false;
      }
    }
    return false;
  }

  static void SetUintValue(zet_value_info_type_exp_t type, uint64_t value, zet_metric_programmable_param_value_exp_t &val) {
    switch (type) {
      case ZET_VALUE_INFO_TYPE_EXP_UINT8:
      case ZET_VALUE_INFO_TYPE_EXP_UINT16:
      case ZET_VALUE_INFO_TYPE_EXP_UINT32:
        val.value.ui32 = static_cast<uint32_t>(value);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_UINT64:
      case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
        val.value.ui64 = value;
        break;
      default:
        UNITRACE_ASSERT(false && "Invalid value type for uint parameter");
    }
  }

  static inline void addParam(const zet_metric_programmable_param_info_exp_t& param, ConfigNodePtr params_set, const std::string &value_range_desc = "") {
    AttributePtr ap ;
    switch (param.valueInfoType) {
      case ZET_VALUE_INFO_TYPE_EXP_UINT8:   // zet_value_t holds 8/16-bit defaults in ui32
      case ZET_VALUE_INFO_TYPE_EXP_UINT16:
      case ZET_VALUE_INFO_TYPE_EXP_UINT32:
        ap = std::make_shared<UInt64Attribute>(param.name, param.defaultValue.ui32, value_range_desc);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_UINT64:
      case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
        ap = std::make_shared<UInt64Attribute>(param.name, param.defaultValue.ui64, value_range_desc);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
          ap = std::make_shared<DoubleAttribute>(param.name, param.defaultValue.fp32, value_range_desc);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
      case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE:
        ap = std::make_shared<DoubleAttribute>(param.name, param.defaultValue.fp64, value_range_desc);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_BOOL8:
        ap = std::make_shared<BoolAttribute>(param.name, param.defaultValue.b8 != 0, value_range_desc);
        break;
      case ZET_VALUE_INFO_TYPE_EXP_FORCE_UINT32:
      default:
        UNITRACE_ASSERT(!"Not supported value type");
        return;
    }
    params_set->addAttribute(ap);
  }

  // The sampling types unitrace collects: event based (-q) and time based (-k).
  // Entries supporting neither are left out of the template.
  static constexpr zet_metric_group_sampling_type_flags_t kCollectableSamplingTypes =
      ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED |
      ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;

  static std::string getSupportedSamplingTypesDesc(zet_metric_group_sampling_type_flags_t samplingTypes) {
    std::string str;

    if (samplingTypes & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED) {
      str += "event_based,";
    }
    if (samplingTypes & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
      str += "time_based,";
    }

    assert(!str.empty());
    str.pop_back(); // remove trailing comma
    return "Supported sampling types: " + str;
  }

  // An entry is enabled only if it has "enable=true"; a missing attribute means disabled.
  static inline bool IsNodeEnabled(const ConfigNodePtr cnode) {
    auto attribute_enable = cnode->findAttributeByKey("enable");
    if (attribute_enable != nullptr) {
       BoolAttribute* bool_attr = dynamic_cast<BoolAttribute*>(attribute_enable.get());
      if(bool_attr == nullptr) {
        std::cerr << "[ERROR] Metric(" << cnode->getName() << ") has invalid \"enable\" attribute." << std::endl;
        return false;
      }

      return bool_attr->getTypedValue();
    }
    return false;
  }

  // Sets val from a scalar config value. Returns "" on success, otherwise the reason.
  // Integer parameters take unsigned integers, decimal parameters take decimals or
  // unsigned integers, BOOL8 parameters take true/false. The value must be one of
  // the reported valid values, or inside one of the reported intervals for *_RANGE
  // types; if the driver reports no valid values, any value of the right type is
  // passed on and the driver validates it. BOOL8 values are not checked against
  // the reported values.
  static std::string SetParamValue(const AttributePtr &aparam, const zet_metric_programmable_param_info_exp_t &info,
                                   const std::vector<zet_metric_programmable_param_value_info_exp_t> &val_info,
                                   zet_metric_programmable_param_value_exp_t &val) {
    const zet_value_info_type_exp_t type = info.valueInfoType;

    if (IsUintValueType(type)) {
      auto attr = std::dynamic_pointer_cast<UInt64Attribute>(aparam);
      if (attr == nullptr) {
        return "expected an unsigned integer, found '" + aparam->getValue() + "'";
      }
      if (!IsUintValueValid(type, val_info, attr->getTypedValue())) {
        return "value " + aparam->getValue() + " is not valid";
      }
      SetUintValue(type, attr->getTypedValue(), val);
      return "";
    }

    if (IsFloatValueType(type)) {
      double value = 0.0;
      if (auto dattr = std::dynamic_pointer_cast<DoubleAttribute>(aparam)) {
        value = dattr->getTypedValue();
      } else if (auto uattr = std::dynamic_pointer_cast<UInt64Attribute>(aparam)) {
        value = static_cast<double>(uattr->getTypedValue());
      } else {
        return "expected a decimal number, found '" + aparam->getValue() + "'";
      }
      if (!IsFloatValueValid(type, val_info, value)) {
        return "value " + aparam->getValue() + " is not valid";
      }
      if (type == ZET_VALUE_INFO_TYPE_EXP_FLOAT32) {
        val.value.fp32 = static_cast<float>(value);
      } else {
        val.value.fp64 = value;
      }
      return "";
    }

    if (type == ZET_VALUE_INFO_TYPE_EXP_BOOL8) {
      auto battr = std::dynamic_pointer_cast<BoolAttribute>(aparam);
      if (battr == nullptr) {
        return "expected true or false, found '" + aparam->getValue() + "'";
      }
      val.value.b8 = battr->getTypedValue();
      return "";
    }

    return "unsupported parameter type " + std::to_string(type);
  }

  static inline void FillDefaultVals(std::vector<zet_metric_programmable_param_info_exp_t> &params, std::vector<zet_metric_programmable_param_value_exp_t> &vals) {
    UNITRACE_ASSERT(params.size() == vals.size());

    for (uint32_t i = 0; i < params.size(); ++i) {
      // no need to check the type, since union is copied to the same union
      vals[i].value = params[i].defaultValue;
    }
  }

 private: //data
  ze_device_handle_t m_device_;
  zet_metric_group_sampling_type_flag_t m_samplingType_;
  std::unordered_map<std::string, zet_metric_group_handle_t> m_predef_groups_;
  struct ProgrammableRef {
    zet_metric_programmable_exp_handle_t handle;
    uint32_t parameterCount;
    std::string name;
  };
  std::unordered_map<std::string, ProgrammableRef> m_programmables_;  // keyed by config key
  std::vector<zet_metric_handle_t> m_all_progs_metrics_;  // created metrics, owned
  // Selected groups: [0, m_prog_groups_start_) are predefined groups owned by the
  // driver; [m_prog_groups_start_, end) are created groups owned by this object.
  // The destructor relies on this order to destroy only the created groups.
  std::vector<zet_metric_group_handle_t> m_metric_groups_;
  uint32_t m_prog_groups_start_;
}; //class ZeMetricsManager

#endif // PTI_TOOLS_UNITRACE_METRICS_CONFIG_H_

