//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Unit test of the --metric-config file parser (ConfigNode.h).

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ConfigNode.h"
#include "metric_config_validator.h"

static int failures = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << ": " #cond      \
                << std::endl;                                                    \
      ++failures;                                                                \
    }                                                                            \
  } while (0)

static std::string TempPath(const std::string& name) {
  return (std::filesystem::temp_directory_path() / ("unitrace_config_node_test_" + name)).string();
}

static std::string WriteText(const std::string& text) {
  std::string path = TempPath("load.txt");
  std::ofstream out(path, std::ios::trunc);
  out << text;
  return path;
}

static ConfigNodePtr LoadText(const std::string& text, std::string* error = nullptr) {
  std::string path = WriteText(text);
  std::string err;
  ConfigNodePtr node = ConfigNode::load(path, err);
  std::filesystem::remove(path);
  if (error != nullptr) {
    *error = err;
  }
  return node;
}

// Loads and validates text; returns the error with the temp file name replaced by "F".
static std::string ValidateText(const std::string& text, uint32_t cli_groups = 0) {
  std::string path = WriteText(text);
  std::string error;
  ConfigNodePtr node = metric_config::Load(path, cli_groups, error);
  std::filesystem::remove(path);
  if (node != nullptr) {
    return "";
  }
  if (error.compare(0, path.size(), path) == 0) {
    error = "F" + error.substr(path.size());
  }
  return error;
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.compare(0, prefix.size(), prefix) == 0;
}

static std::vector<uint64_t> Values(const U64RangeListAttribute& attr) {
  std::vector<uint64_t> values;
  uint64_t v = attr.getMin();
  do {
    values.push_back(v);
  } while (attr.getNext(v, v) && values.size() < 1000);
  return values;
}

static std::vector<uint64_t> RangeValues(const std::string& str) {
  auto ranges = U64Range::fromString(str);
  if (ranges.empty()) {
    return {};
  }
  return Values(U64RangeListAttribute("x", ranges));
}

static void TestValidConfig() {
  ConfigNodePtr root = LoadText(
      "# full-line comment\n"
      "predefined{\n"
      "    ComputeBasic{\n"
      "        enable=true   # inline comment\n"
      "    }\n"
      "}\n"
      "programmable{\n"
      "    METRIC_A{\n"
      "        enable = false\n"
      "        parameters{\n"
      "            Count=7\n"
      "            Ratio=0.5\n"
      "            Offset=-3\n"
      "            Label='a # b'\n"
      "            Disaggregation=[0,1,2,10:12]\n"
      "        }\n"
      "    }\n"
      "}\n");
  CHECK(root != nullptr);
  if (root == nullptr) {
    return;
  }
  CHECK(root->getChildren().size() == 2);

  auto predef = root->findChildByName("predefined");
  CHECK(predef != nullptr);
  auto group = predef->findChildByName("ComputeBasic");
  CHECK(group != nullptr);
  auto enable = std::dynamic_pointer_cast<BoolAttribute>(group->findAttributeByKey("enable"));
  CHECK(enable != nullptr && enable->getTypedValue());

  auto metric = root->findChildByName("programmable")->findChildByName("METRIC_A");
  CHECK(metric != nullptr);
  auto disabled = std::dynamic_pointer_cast<BoolAttribute>(metric->findAttributeByKey("enable"));
  CHECK(disabled != nullptr && !disabled->getTypedValue());

  auto params = metric->findChildByName("parameters");
  CHECK(params != nullptr);
  CHECK(params->getAttributes().size() == 5);
  CHECK(params->findAttributeByKey("Count")->getType() == AttributeType::UNSIGNED_64);
  CHECK(params->findAttributeByKey("Count")->getValue() == "7");
  CHECK(params->findAttributeByKey("Ratio")->getType() == AttributeType::DOUBLE);
  CHECK(params->findAttributeByKey("Offset")->getType() == AttributeType::INT);
  CHECK(params->findAttributeByKey("Label")->getType() == AttributeType::STRING);
  CHECK(params->findAttributeByKey("Label")->getValue() == "a # b");

  auto range = std::dynamic_pointer_cast<U64RangeListAttribute>(params->findAttributeByKey("Disaggregation"));
  CHECK(range != nullptr);
  if (range != nullptr) {
    CHECK(Values(*range) == (std::vector<uint64_t>{0, 1, 2, 10, 11, 12}));
    CHECK(range->getValue() == "[0,1,2,10:12]");
  }
}

static void TestInvalidConfig() {
  CHECK(LoadText("a{\n") == nullptr);                    // unmatched opening bracket
  CHECK(LoadText("}\n") == nullptr);                     // unmatched closing bracket
  CHECK(LoadText("a{\n  novalue\n}\n") == nullptr);      // no '='
  CHECK(LoadText("a{\n  x=\n}\n") == nullptr);           // empty value
  CHECK(LoadText("a{\n  =1\n}\n") == nullptr);           // empty key
  CHECK(LoadText("a{\n  x=12abc\n}\n") == nullptr);      // trailing garbage
  CHECK(LoadText("a{\n  x=1.5abc\n}\n") == nullptr);
  CHECK(LoadText("a{\n  x=-4x\n}\n") == nullptr);
  CHECK(LoadText("a{\n  x=[1,,2]\n}\n") == nullptr);     // empty range item
  CHECK(LoadText("a{\n  x=[]\n}\n") == nullptr);
  CHECK(LoadText("a{\n  x=[3:1]\n}\n") == nullptr);      // min > max
  CHECK(LoadText("a{\n  x=[0:4:0]\n}\n") == nullptr);    // zero step
  CHECK(LoadText("a{\n  x=[0:5,3]\n}\n") == nullptr);    // overlapping
  CHECK(LoadText("a{\n  x=[-1]\n}\n") == nullptr);       // negative
  CHECK(LoadText("a{\n  x=[1:2:3:4]\n}\n") == nullptr);  // too many fields
  CHECK(ConfigNode::load(TempPath("does_not_exist.txt")) == nullptr);
}

static void TestErrorMessages() {
  std::string error;
  CHECK(LoadText("a{\n  x=12abc\n}\n", &error) == nullptr);
  CHECK(error.find(":2: invalid value '12abc' for 'x'") != std::string::npos);
  CHECK(LoadText("a{\n b{\n }\n", &error) == nullptr);
  CHECK(error.find(":1: 'a{' is not closed") != std::string::npos);
  CHECK(LoadText("}\n", &error) == nullptr);
  CHECK(error.find(":1: '}' without a matching '{'") != std::string::npos);
  CHECK(LoadText("a b{\n}\n", &error) == nullptr);
  CHECK(error.find(":1: invalid node name 'a b'") != std::string::npos);
  CHECK(LoadText("a{\n x=[1:3\n}\n", &error) == nullptr);
  CHECK(error.find(":2: invalid range list '[1:3' for 'x'") != std::string::npos);
  CHECK(ConfigNode::load(TempPath("does_not_exist.txt"), error) == nullptr);
  CHECK(error.find("cannot open file") != std::string::npos);

  // Line numbers are recorded.
  ConfigNodePtr root = LoadText("\n# c\nprogrammable{\n  M{\n    enable=true\n  }\n}\n");
  CHECK(root != nullptr);
  if (root != nullptr) {
    auto m = root->findChildByName("programmable")->findChildByName("M");
    CHECK(m->getLine() == 4);
    CHECK(m->findAttributeByKey("enable")->getLine() == 5);
  }
}

static void TestValidation() {
  const std::string ok_prog = "programmable{\n M{\n  enable=true\n  parameters{\n   P=[0:3]\n   Q=1\n  }\n }\n}\n";
  CHECK(ValidateText(ok_prog) == "");
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n }\n H{\n  enable=false\n }\n}\n") == "");
  CHECK(ValidateText("predefined{\n G{\n }\n}\n", 1) == "");  // -g group, nothing enabled in file
  // Several programmable metrics are collected as one group.
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n }\n N{\n  enable=true\n }\n}\n") == "");

  CHECK(ValidateText("") == "F: no 'predefined' or 'programmable' node found");
  CHECK(ValidateText("x=1\n") == "F:1: 'x' must be inside a 'predefined' or 'programmable' node");
  CHECK(ValidateText("metrics{\n}\n") == "F:1: unknown node 'metrics' (expected 'predefined' or 'programmable')");
  CHECK(ValidateText("predefined{\n}\npredefined{\n}\n") == "F:3: duplicate 'predefined' node (first at line 1)");
  CHECK(ValidateText("predefined{\n enable=true\n}\n") ==
        "F:2: 'enable' must be inside a predefined group node of 'predefined'");
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n }\n G{\n }\n}\n") ==
        "F:5: duplicate predefined group 'G' (first at line 2)");
  CHECK(ValidateText("predefined{\n G{\n  enable=1\n }\n}\n") ==
        "F:3: 'enable' of predefined group 'G' must be true or false, found '1'");
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n  enable=true\n }\n}\n") ==
        "F:4: duplicate 'enable' in predefined group 'G'");
  CHECK(ValidateText("predefined{\n G{\n  enabled=true\n }\n}\n") ==
        "F:3: unknown attribute 'enabled' in predefined group 'G' (only 'enable' is allowed)");
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n  parameters{\n  }\n }\n}\n") ==
        "F:4: unexpected node 'parameters' in predefined group 'G' (only 'enable' is allowed)");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  P=1\n }\n}\n") ==
        "F:4: unknown attribute 'P' in programmable metric 'M' (only 'enable' is allowed, parameters go into a 'parameters' node)");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  params{\n  }\n }\n}\n") ==
        "F:4: unexpected node 'params' in programmable metric 'M' (only 'parameters' is allowed)");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n  }\n  parameters{\n  }\n }\n}\n") ==
        "F:6: duplicate 'parameters' node in programmable metric 'M'");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n   P=1\n   P=2\n  }\n }\n}\n") ==
        "F:6: duplicate parameter 'P' of 'M' (first set at line 5)");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n   P=[0:1]\n   Q=[2,3]\n  }\n }\n}\n") ==
        "F:6: only one parameter of 'M' can be a range list ('P' at line 5 is one already)");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n   P='x'\n  }\n }\n}\n") ==
        "F:5: parameter 'P' of 'M': string values are not supported");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n   P=-1\n  }\n }\n}\n") ==
        "F:5: parameter 'P' of 'M': negative values are not supported");
  CHECK(ValidateText("programmable{\n M{\n  enable=true\n  parameters{\n   S{\n   }\n  }\n }\n}\n") ==
        "F:5: unexpected node 'S' in 'parameters' of 'M' (only <name>=<value> lines are allowed)");

  CHECK(ValidateText("predefined{\n G{\n  enable=false\n }\n}\n") ==
        "F: no metric group or programmable metric is enabled (set enable=true)");
  // Several enabled groups are accepted here (one file can serve several devices);
  // CreateMetricGroups checks that exactly one is left per device.
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n }\n H{\n  enable=true\n }\n}\n") == "");
  CHECK(ValidateText("predefined{\n G{\n  enable=true\n }\n}\n" + ok_prog) == "");
  CHECK(ValidateText(ok_prog, 1) == "");

  // Syntax errors are reported by Load as well.
  CHECK(StartsWith(ValidateText("predefined{\n"), "F:1: 'predefined{' is not closed"));

  CHECK(metric_config::CountGroups("") == 0);
  CHECK(metric_config::CountGroups("A") == 1);
  CHECK(metric_config::CountGroups("A,B") == 2);
}

static void TestExampleFile() {
  std::string error;
  ConfigNodePtr root = metric_config::Load(EXAMPLE_METRIC_CONFIG, 0, error);
  if (root == nullptr) {
    std::cerr << "[FAILED] example: " << error << std::endl;
  }
  CHECK(root != nullptr);
}

static void TestRanges() {
  CHECK(RangeValues("5") == (std::vector<uint64_t>{5}));
  CHECK(RangeValues("0:3") == (std::vector<uint64_t>{0, 1, 2, 3}));
  CHECK(RangeValues("0:16:4") == (std::vector<uint64_t>{0, 4, 8, 12, 16}));
  // The end of a stepped range is not a value unless it is on the step.
  CHECK(RangeValues("0:9:4") == (std::vector<uint64_t>{0, 4, 8}));
  CHECK(RangeValues("0:9:4,20") == (std::vector<uint64_t>{0, 4, 8, 20}));
  CHECK(RangeValues(" 1 , 3 : 4 ") == (std::vector<uint64_t>{1, 3, 4}));
  CHECK(RangeValues("18446744073709551614:18446744073709551615") ==
        (std::vector<uint64_t>{18446744073709551614ull, 18446744073709551615ull}));
  CHECK(RangeValues("0:18446744073709551615:18446744073709551615") ==
        (std::vector<uint64_t>{0, 18446744073709551615ull}));
  CHECK(U64Range::fromString("18446744073709551616").empty());  // overflow
  CHECK(U64Range::fromString("1,").empty());
  CHECK(U64Range::fromString("+1").empty());
}

// Decimals must survive save/load exactly: generated defaults are compared exactly
// against the values the driver reports.
static void TestDoubleRoundTrip() {
  const std::vector<double> values = {1.0 / 3, 1e-7, static_cast<double>(0.1f), 0.1, 1.0, 0.0, -0.5,
                                      1e20, 123456789.125, 2.5e-300, 1.7976931348623157e308};
  ConfigNodePtr root = std::make_shared<ConfigNode>("");
  ConfigNodePtr node = std::make_shared<ConfigNode>("n");
  root->addChild(node);
  for (size_t i = 0; i < values.size(); ++i) {
    auto attr = std::make_shared<DoubleAttribute>("v" + std::to_string(i), values[i]);
    CHECK(attr->getValue().find('.') != std::string::npos);  // read back as a decimal
    node->addAttribute(attr);
  }
  CHECK(DoubleAttribute("x", 1.0 / 3).getValue() == "0.3333333333333333");
  CHECK(DoubleAttribute("x", 0.5).getValue() == "0.5");
  CHECK(DoubleAttribute("x", 1.0).getValue() == "1.0");
  CHECK(DoubleAttribute("x", 1e20).getValue() == "1.0e+20");

  std::string path = TempPath("doubles.txt");
  CHECK(ConfigNode::save(root, path));
  std::string error;
  ConfigNodePtr loaded = ConfigNode::load(path, error);
  std::filesystem::remove(path);
  CHECK(loaded != nullptr);
  if (loaded == nullptr) {
    std::cerr << error << std::endl;
    return;
  }
  auto lnode = loaded->findChildByName("n");
  for (size_t i = 0; i < values.size(); ++i) {
    auto attr = std::dynamic_pointer_cast<DoubleAttribute>(lnode->findAttributeByKey("v" + std::to_string(i)));
    CHECK(attr != nullptr && attr->getTypedValue() == values[i]);
  }
}

static void TestSaveLoadRoundTrip() {
  ConfigNodePtr root = std::make_shared<ConfigNode>("");
  ConfigNodePtr prog = std::make_shared<ConfigNode>("programmable");
  ConfigNodePtr metric = std::make_shared<ConfigNode>("METRIC_B");
  ConfigNodePtr params = std::make_shared<ConfigNode>("parameters");
  root->addChild(prog);
  prog->addChild(metric);
  metric->addAttribute(std::make_shared<BoolAttribute>("enable", true, "Supported sampling types: time_based"));
  metric->addChild(params);
  params->addAttribute(std::make_shared<UInt64Attribute>("Count", 3, "Valid values: 0:19"));
  params->addAttribute(std::make_shared<StringAttribute>("Label", "x#y"));
  params->addAttribute(std::make_shared<U64RangeListAttribute>("Range", U64Range::fromString("0:8:2")));

  std::string path = TempPath("roundtrip.txt");
  CHECK(ConfigNode::save(root, path));
  ConfigNodePtr loaded = ConfigNode::load(path);
  std::filesystem::remove(path);
  CHECK(loaded != nullptr);
  if (loaded == nullptr) {
    return;
  }
  auto lmetric = loaded->findChildByName("programmable")->findChildByName("METRIC_B");
  CHECK(lmetric != nullptr);
  auto lparams = lmetric->findChildByName("parameters");
  CHECK(lparams->findAttributeByKey("Count")->getValue() == "3");
  CHECK(lparams->findAttributeByKey("Label")->getValue() == "x#y");
  CHECK(lparams->findAttributeByKey("Range")->getValue() == "[0:8:2]");

  std::string bad_path = (std::filesystem::temp_directory_path() / "no_such_dir_unitrace" / "x.txt").string();
  CHECK(!ConfigNode::save(root, bad_path));
}

int main() {
  TestValidConfig();
  TestInvalidConfig();
  TestErrorMessages();
  TestValidation();
  TestExampleFile();
  TestRanges();
  TestSaveLoadRoundTrip();
  TestDoubleRoundTrip();
  if (failures != 0) {
    std::cerr << "[ERROR] " << failures << " check(s) failed" << std::endl;
    return 1;
  }
  std::cout << "[INFO] All config node tests passed" << std::endl;
  return 0;
}
