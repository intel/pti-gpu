//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_METRIC_CONFIG_VALIDATOR_H_
#define PTI_TOOLS_UNITRACE_METRIC_CONFIG_VALIDATOR_H_

// Structural validation of a --metric-config file (see README, "Metric Config
// File Format"). It needs no device, so unitrace checks the file before the
// application starts. Whether the named groups, programmables and parameter
// values exist on a device is checked later, when the metric groups are created.

#include <map>
#include <string>

#include "ConfigNode.h"

namespace metric_config {

inline const char* kPredefined = "predefined";
inline const char* kProgrammable = "programmable";
inline const char* kParameters = "parameters";
inline const char* kEnable = "enable";

namespace detail {

inline std::string Where(const std::string& filename, uint32_t line) {
  return line != 0 ? filename + ":" + std::to_string(line) + ": " : filename + ": ";
}

// Validates the "enable" attribute and the absence of other attributes of a
// predefined group or programmable metric entry. Sets enabled.
inline bool CheckEntryAttributes(const ConfigNodePtr& entry, const char* kind, const std::string& filename,
                                 bool& enabled, std::string& error) {
  enabled = false;
  bool seen = false;
  for (const auto& attr : entry->getAttributes()) {
    if (attr->getKey() != kEnable) {
      error = Where(filename, attr->getLine()) + "unknown attribute '" + attr->getKey() + "' in " + kind +
              " '" + entry->getName() + "' (only 'enable' is allowed" +
              (std::string(kind) == "programmable metric" ? ", parameters go into a 'parameters' node)" : ")");
      return false;
    }
    if (seen) {
      error = Where(filename, attr->getLine()) + "duplicate 'enable' in " + kind + " '" + entry->getName() + "'";
      return false;
    }
    seen = true;
    auto bool_attr = std::dynamic_pointer_cast<BoolAttribute>(attr);
    if (bool_attr == nullptr) {
      error = Where(filename, attr->getLine()) + "'enable' of " + kind + " '" + entry->getName() +
              "' must be true or false, found '" + attr->getValue() + "'";
      return false;
    }
    enabled = bool_attr->getTypedValue();
  }
  return true;
}

inline bool CheckParameters(const ConfigNodePtr& params, const ConfigNodePtr& entry, const std::string& filename,
                            std::string& error) {
  if (!params->getChildren().empty()) {
    const auto& child = params->getChildren().front();
    error = Where(filename, child->getLine()) + "unexpected node '" + child->getName() + "' in 'parameters' of '" +
            entry->getName() + "' (only <name>=<value> lines are allowed)";
    return false;
  }
  std::map<std::string, uint32_t> keys;
  AttributePtr range_param;
  for (const auto& attr : params->getAttributes()) {
    auto it = keys.find(attr->getKey());
    if (it != keys.end()) {
      error = Where(filename, attr->getLine()) + "duplicate parameter '" + attr->getKey() + "' of '" +
              entry->getName() + "' (first set at line " + std::to_string(it->second) + ")";
      return false;
    }
    keys[attr->getKey()] = attr->getLine();

    switch (attr->getType()) {
      case AttributeType::STRING:
        error = Where(filename, attr->getLine()) + "parameter '" + attr->getKey() + "' of '" + entry->getName() +
                "': string values are not supported";
        return false;
      case AttributeType::INT:
        error = Where(filename, attr->getLine()) + "parameter '" + attr->getKey() + "' of '" + entry->getName() +
                "': negative values are not supported";
        return false;
      case AttributeType::UNSIGNED_64_RANGE_LIST:
        if (range_param != nullptr) {
          error = Where(filename, attr->getLine()) + "only one parameter of '" + entry->getName() +
                  "' can be a range list ('" + range_param->getKey() + "' at line " +
                  std::to_string(range_param->getLine()) + " is one already)";
          return false;
        }
        range_param = attr;
        break;
      default:
        break;
    }
  }
  return true;
}

} // namespace detail

// Validates the structure of a loaded metric config:
//   predefined{ <group>{ enable=<bool> } ... }
//   programmable{ <metric>{ enable=<bool>  parameters{ <name>=<value> ... } } ... }
// Both sections are optional, but at least one must be present; each may appear
// once. Entry names must be unique within a section, "enable" defaults to false,
// at most one parameter per metric may be a range list, and parameter values must
// be unsigned integers, decimals, booleans or range lists. Something must be
// selected: a group given with -g (cli_group_count) or an enabled entry.
// Selecting more than one group is not rejected here: entries a device does not
// support are skipped when the groups are created, so one file can serve several
// devices; CreateMetricGroups checks that exactly one group is left per device.
// Returns false with error = "<filename>[:<line>]: <reason>" if the config is invalid.
inline bool Validate(const ConfigNodePtr& root, const std::string& filename, uint32_t cli_group_count,
                     std::string& error) {
  using detail::Where;
  error.clear();

  if (!root->getAttributes().empty()) {
    const auto& attr = root->getAttributes().front();
    error = Where(filename, attr->getLine()) + "'" + attr->getKey() +
            "' must be inside a 'predefined' or 'programmable' node";
    return false;
  }

  std::map<std::string, uint32_t> sections;
  uint32_t enabled_entries = 0;

  for (const auto& section : root->getChildren()) {
    const std::string& sname = section->getName();
    if (sname != kPredefined && sname != kProgrammable) {
      error = Where(filename, section->getLine()) + "unknown node '" + sname +
              "' (expected 'predefined' or 'programmable')";
      return false;
    }
    auto sit = sections.find(sname);
    if (sit != sections.end()) {
      error = Where(filename, section->getLine()) + "duplicate '" + sname + "' node (first at line " +
              std::to_string(sit->second) + ")";
      return false;
    }
    sections[sname] = section->getLine();

    const bool predefined = (sname == kPredefined);
    const char* kind = predefined ? "predefined group" : "programmable metric";

    if (!section->getAttributes().empty()) {
      const auto& attr = section->getAttributes().front();
      error = Where(filename, attr->getLine()) + "'" + attr->getKey() + "' must be inside a " + kind +
              " node of '" + sname + "'";
      return false;
    }

    std::map<std::string, uint32_t> entries;
    for (const auto& entry : section->getChildren()) {
      auto eit = entries.find(entry->getName());
      if (eit != entries.end()) {
        error = Where(filename, entry->getLine()) + "duplicate " + kind + " '" + entry->getName() +
                "' (first at line " + std::to_string(eit->second) + ")";
        return false;
      }
      entries[entry->getName()] = entry->getLine();

      bool enabled = false;
      if (!detail::CheckEntryAttributes(entry, kind, filename, enabled, error)) {
        return false;
      }

      if (predefined) {
        if (!entry->getChildren().empty()) {
          const auto& child = entry->getChildren().front();
          error = Where(filename, child->getLine()) + "unexpected node '" + child->getName() +
                  "' in predefined group '" + entry->getName() + "' (only 'enable' is allowed)";
          return false;
        }
      } else {
        bool has_params = false;
        for (const auto& child : entry->getChildren()) {
          if (child->getName() != kParameters) {
            error = Where(filename, child->getLine()) + "unexpected node '" + child->getName() +
                    "' in programmable metric '" + entry->getName() + "' (only 'parameters' is allowed)";
            return false;
          }
          if (has_params) {
            error = Where(filename, child->getLine()) + "duplicate 'parameters' node in programmable metric '" +
                    entry->getName() + "'";
            return false;
          }
          has_params = true;
          if (!detail::CheckParameters(child, entry, filename, error)) {
            return false;
          }
        }
      }

      if (enabled) {
        ++enabled_entries;
      }
    }
  }

  if (sections.empty()) {
    error = Where(filename, 0) + "no 'predefined' or 'programmable' node found";
    return false;
  }

  if (cli_group_count + enabled_entries == 0) {
    error = Where(filename, 0) + "no metric group or programmable metric is enabled (set enable=true)";
    return false;
  }
  return true;
}

// Loads and validates a metric config file. Returns nullptr with error set to
// "<filename>[:<line>]: <reason>" on a syntax or structure error.
inline ConfigNodePtr Load(const std::string& filename, uint32_t cli_group_count, std::string& error) {
  ConfigNodePtr root = ConfigNode::load(filename, error);
  if (root == nullptr || !Validate(root, filename, cli_group_count, error)) {
    return nullptr;
  }
  return root;
}

// Number of groups in a comma-separated -g/--group value.
inline uint32_t CountGroups(const std::string& groups_list) {
  if (groups_list.empty()) {
    return 0;
  }
  uint32_t count = 1;
  for (char c : groups_list) {
    if (c == ',') {
      ++count;
    }
  }
  return count;
}

} // namespace metric_config

#endif // PTI_TOOLS_UNITRACE_METRIC_CONFIG_VALIDATOR_H_
