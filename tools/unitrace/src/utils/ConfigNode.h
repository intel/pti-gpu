//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef CONFIG_NODE_H
#define CONFIG_NODE_H

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

enum class AttributeType{
  STRING,
  UNSIGNED_64,
  INT,
  DOUBLE,
  BOOL,
  UNSIGNED_64_RANGE_LIST
};

namespace config_node_detail {

inline std::string Trim(const std::string& input) {
  size_t begin = 0;
  while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(begin, end - begin);
}

// Strict decimal parse: the whole string must be digits and fit in uint64_t.
inline bool ParseU64(const std::string& str, uint64_t& out) {
  if (str.empty() ||
      !std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isdigit(c); })) {
    return false;
  }
  try {
    size_t pos = 0;
    out = std::stoull(str, &pos, 10);
    return pos == str.size();
  } catch (const std::exception&) {
    return false;
  }
}

// Shortest decimal text that parses back to exactly value. Always contains a '.'
// (e.g. "1.0", "1.0e+20") so that ConfigNode::load reads it back as a decimal.
inline std::string FormatDouble(double value) {
  char buf[64];
  for (int precision = 15; precision <= 17; ++precision) {
    snprintf(buf, sizeof(buf), "%.*g", precision, value);
    if (strtod(buf, nullptr) == value) {
      break;
    }
  }
  std::string str(buf);
  if (str.find('.') == std::string::npos) {
    size_t exp = str.find_first_of("eE");
    if (exp != std::string::npos) {
      str.insert(exp, ".0");
    } else if (str.find_first_of("ni") == std::string::npos) {  // not inf/nan
      str += ".0";
    }
  }
  return str;
}

// Removes a trailing "# comment", ignoring '#' inside single-quoted strings.
inline std::string StripComment(const std::string& line) {
  bool in_quotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '\'') {
      in_quotes = !in_quotes;
    } else if (line[i] == '#' && !in_quotes) {
      return line.substr(0, i);
    }
  }
  return line;
}

// Node names and attribute keys: non-empty, no whitespace or syntax characters.
inline bool IsValidName(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  for (unsigned char c : name) {
    if (std::isspace(c) || c == '{' || c == '}' || c == '=' || c == '#' ||
        c == '\'' || c == '[' || c == ']') {
      return false;
    }
  }
  return true;
}

} // namespace config_node_detail

class Attribute;
typedef std::shared_ptr<Attribute> AttributePtr;

class Attribute{
  public:
    Attribute(const std::string& key, AttributeType type, const std::string &desc) :
      key(key), type(type), desc(desc) {}

    virtual ~Attribute() = default;

    std::string getKey() const{
      return key;
    }

    AttributeType getType() const{
      return type;
    }

    virtual std::string getValue() const = 0;

    const std::string &getDesc() const{
      return desc;
    }

    // Line in the loaded file; 0 if not loaded from a file.
    uint32_t getLine() const{
      return line;
    }

    void setLine(uint32_t l){
      line = l;
    }

  private:
    std::string key;
    AttributeType type;
    std::string desc;
    uint32_t line = 0;
};

template <typename T>
class TemplateAttribute : public Attribute{
  public:
    TemplateAttribute(const std::string& key, const T& value , AttributeType type, const std::string &desc = "") : Attribute(key, type, desc), value(value) {}

    std::string getValue() const override{
      return ConvertToString(value);
    }

    void setValue(const T& value)  {
      this->value = value;
    }

    T getTypedValue() const{
      return value;
    }

  protected:
    static std::string ConvertToString(const std::string& value) {
      return value;
    }

    static std::string ConvertToString(const bool& value) {
      return value ? "true" : "false";
    }

    static std::string ConvertToString(const double& value) {
      return config_node_detail::FormatDouble(value);
    }

    template <typename U>
    static std::string ConvertToString(const U& value) {
      return std::to_string(value);
    }

    T value;
};

class StringAttribute : public TemplateAttribute<std::string>{
  public:
    StringAttribute(const std::string& key, const std::string& value, const std::string &desc = "") : TemplateAttribute<std::string>(key, value, AttributeType::STRING, desc) {}
};

class IntAttribute : public TemplateAttribute<int>{
  public:
    IntAttribute(const std::string& key, int value, const std::string &desc = "") : TemplateAttribute<int>(key, value, AttributeType::INT, desc) {}
};

class UInt64Attribute : public TemplateAttribute<uint64_t>{
  public:
    UInt64Attribute(const std::string& key, uint64_t value, const std::string &desc = "") : TemplateAttribute<uint64_t>(key, value, AttributeType::UNSIGNED_64, desc) {}
};

class DoubleAttribute : public TemplateAttribute<double>{
  public:
    DoubleAttribute(const std::string& key, double value, const std::string &desc = "") : TemplateAttribute<double>(key, value, AttributeType::DOUBLE, desc) {}
};

class BoolAttribute : public TemplateAttribute<bool>{
  public:
    BoolAttribute(const std::string& key, bool value, const std::string &desc = "") : TemplateAttribute<bool>(key, value, AttributeType::BOOL, desc) {}
};

struct U64Range {
  uint64_t min;
  uint64_t max;
  uint64_t step;

  U64Range(uint64_t min, uint64_t max, uint64_t step) : min(min), max(max), step(step) {}

  // Parses "a,b:c,d:e:f" (step defaults to 1). Ranges must be ascending and
  // non-overlapping. Returns an empty vector on any syntax error.
  static std::vector<U64Range> fromString(const std::string& str) {
    std::vector<U64Range> ranges;
    size_t start = 0;
    while (start <= str.size()) {
      size_t end = str.find(',', start);
      if (end == std::string::npos) {
        end = str.size();
      }
      std::string rangeStr = config_node_detail::Trim(str.substr(start, end - start));

      std::vector<std::string> parts;
      size_t pstart = 0;
      while (true) {
        size_t colon = rangeStr.find(':', pstart);
        parts.push_back(config_node_detail::Trim(rangeStr.substr(pstart, colon == std::string::npos ? std::string::npos : colon - pstart)));
        if (colon == std::string::npos) {
          break;
        }
        pstart = colon + 1;
      }

      uint64_t min = 0;
      uint64_t max = 0;
      uint64_t step = 1;
      if (parts.size() > 3 ||
          !config_node_detail::ParseU64(parts[0], min) ||
          (parts.size() >= 2 && !config_node_detail::ParseU64(parts[1], max)) ||
          (parts.size() == 3 && !config_node_detail::ParseU64(parts[2], step))) {
        return {};
      }
      if (parts.size() == 1) {
        max = min;
      }

      if (min > max || step == 0 ||
          (!ranges.empty() && min <= ranges.back().max)) {
        return {};
      }

      ranges.emplace_back(min, max, step);
      start = end + 1;
    }
    return ranges;
  }
};

class U64RangeListAttribute : public Attribute{
  public:

    U64RangeListAttribute(const std::string& key,
                          const std::vector<U64Range>& ranges,
                          const std::string &desc = "") :
      Attribute(key, AttributeType::UNSIGNED_64_RANGE_LIST, desc),
      ranges_(ranges) {
      assert(!ranges_.empty());
    }

    std::string getValue() const override{
      std::string result = "[";
      for (const auto& range : ranges_) {
        result += std::to_string(range.min);
        if (range.max > range.min) {
          result += ":" + std::to_string(range.max);
          if (range.step != 1) {
            result += ":" + std::to_string(range.step);
          }
        }
        if (&range != &ranges_.back()) {
          result += ",";
        }
      }
      result += "]";
      return result;
    }

    uint64_t getMin() const {
      return ranges_.front().min;
    }
    uint64_t getMax() const {
      return ranges_.back().max;
    }
    // Sets next to the value following current. Returns false when current is the last value.
    bool getNext(uint64_t current, uint64_t& next) const {
      for (const auto& range : ranges_) {
        if (current < range.min) {
          next = range.min;
          return true;
        }
        if (current < range.max && range.max - current >= range.step) {
          next = current + range.step;
          return true;
        }
      }
      return false;
    }
private:
    std::vector<U64Range> ranges_;
};

class ConfigNode;
typedef std::shared_ptr<ConfigNode> ConfigNodePtr;

class ConfigNode{
  public:
    // Parses a config file. The syntax is:
    //   <name>{            opens a node (one per line)
    //   }                  closes the innermost node
    //   <key>=<value>      attribute of the innermost node
    //   # ...              comment, to the end of the line (not inside '...')
    // A value is true/false, an unsigned integer, a negative integer, a decimal
    // number (contains '.'), a quoted string '...', or a range list [a,b:c,d:e:f].
    // Returns the root node (unnamed, holding the top-level nodes), or nullptr with
    // error set to "<filename>:<line>: <reason>". Only the syntax is checked here;
    // see metric_config_validator.h for the structure of a metric config file.
    static ConfigNodePtr load(const std::string& filename, std::string& error){
      error.clear();
      std::ifstream fp(filename);
      if (!fp.is_open()) {
        error = filename + ": cannot open file";
        return nullptr;
      }

      std::vector<ConfigNodePtr> nodeStack;
      ConfigNodePtr root = std::make_shared<ConfigNode>("");
      nodeStack.push_back(root);
      uint32_t lineNumber = 0;

      auto fail = [&](const std::string& reason) -> ConfigNodePtr {
        error = filename + ":" + std::to_string(lineNumber) + ": " + reason;
        return nullptr;
      };

      std::string line;
      while (std::getline(fp, line)) {
        ++lineNumber;

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
          line.pop_back();
        }

        std::string content = config_node_detail::Trim(config_node_detail::StripComment(line));
        if (content.empty()) {
          continue;
        }

        if (content == "}") {
          if (nodeStack.size() <= 1) {
            return fail("'}' without a matching '{'");
          }
          nodeStack.pop_back();
          continue;
        }
        ConfigNodePtr currentNode = nodeStack.back();

        if (content.back() == '{') {
          std::string childName = config_node_detail::Trim(content.substr(0, content.size() - 1));
          if (!config_node_detail::IsValidName(childName)) {
            return fail("invalid node name '" + childName + "'");
          }
          ConfigNodePtr child = std::make_shared<ConfigNode>(childName);
          child->line = lineNumber;
          currentNode->addChild(child);
          nodeStack.push_back(child);
          continue;
        }

        size_t equalPos = content.find('=');
        if (equalPos == std::string::npos) {
          return fail("expected '<name>{', '}' or '<key>=<value>', found '" + content + "'");
        }

        std::string key = config_node_detail::Trim(content.substr(0, equalPos));
        std::string value = config_node_detail::Trim(content.substr(equalPos + 1));

        if (!config_node_detail::IsValidName(key)) {
          return fail("invalid key '" + key + "'");
        }
        if (value.empty()) {
          return fail("missing value for '" + key + "'");
        }

        AttributePtr attr;
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
          attr = std::make_shared<StringAttribute>(key, value.substr(1, value.size() - 2));
        } else if (value.front() == '[' || value.back() == ']') {
          std::vector<U64Range> ranges;
          if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
            ranges = U64Range::fromString(value.substr(1, value.size() - 2));
          }
          if (ranges.empty()) {
            return fail("invalid range list '" + value + "' for '" + key +
                        "' (expected e.g. [0,2,4:8] or [0:16:4], ascending and not overlapping)");
          }
          attr = std::make_shared<U64RangeListAttribute>(key, ranges);
        } else if (value == "true") {
          attr = std::make_shared<BoolAttribute>(key, true);
        } else if (value == "false") {
          attr = std::make_shared<BoolAttribute>(key, false);
        } else if (value.find('.') != std::string::npos) {
          try {
            size_t pos = 0;
            double doubleValue = std::stod(value, &pos);
            if (pos != value.size()) {
              throw std::invalid_argument(value);
            }
            attr = std::make_shared<DoubleAttribute>(key, doubleValue);
          } catch (const std::exception&) {
            return fail("invalid decimal value '" + value + "' for '" + key + "'");
          }
        } else if (value.front() == '-') {
          try {
            size_t pos = 0;
            int intValue = std::stoi(value, &pos);
            if (pos != value.size()) {
              throw std::invalid_argument(value);
            }
            attr = std::make_shared<IntAttribute>(key, intValue);
          } catch (const std::exception&) {
            return fail("invalid integer value '" + value + "' for '" + key + "'");
          }
        } else {
          uint64_t uint64Value = 0;
          if (!config_node_detail::ParseU64(value, uint64Value)) {
            return fail("invalid value '" + value + "' for '" + key +
                        "' (expected true/false, a number, '<string>' or a range list)");
          }
          attr = std::make_shared<UInt64Attribute>(key, uint64Value);
        }
        attr->setLine(lineNumber);
        currentNode->addAttribute(attr);
      }

      if (nodeStack.size() > 1) {
        lineNumber = nodeStack.back()->line;
        return fail("'" + nodeStack.back()->name + "{' is not closed");
      }

      return root;
    }

    // Same as load(filename, error), printing the error to stderr.
    static ConfigNodePtr load(const std::string& filename){
      std::string error;
      ConfigNodePtr root = load(filename, error);
      if (root == nullptr) {
        fprintf(stderr, "[ERROR] %s\n", error.c_str());
      }
      return root;
    }

    // Returns false if the file cannot be created or written.
    static bool save(const ConfigNodePtr root, const std::string& filename){
      //save the configNode object to file in the same format as described in load function
      FILE *fp = fopen(filename.c_str(), "wt");
      if (!fp) {
        fprintf(stderr, "Could not open %s for writing\n", filename.c_str());
        return false;
      }

      std::function<void(const ConfigNodePtr, int)> writeConfigNode = [&](const ConfigNodePtr node, int indent) {

        fprintf(fp, "%*s%s{\n", indent, "", node->name.c_str());

        for (const auto& attr : node->getAttributes()) {
          if (attr->getDesc().size() > 0) {
            fprintf(fp, "%*s# %s\n", indent + 4, "", attr->getDesc().c_str());
          }

          if (attr->getType() == AttributeType::STRING) {
            fprintf(fp, "%*s%s='%s'\n", indent + 4, "", attr->getKey().c_str(), attr->getValue().c_str());
          } else {
            fprintf(fp, "%*s%s=%s\n", indent + 4, "", attr->getKey().c_str(), attr->getValue().c_str());
          }
        }
        for (const auto& child : node->getChildren()) {
          writeConfigNode(child, indent + 4);
        }

        fprintf(fp, "%*s}\n", indent, "");
      };

      for (const auto& child : root->getChildren()) {
        writeConfigNode(child, 0);
      }

      bool ok = (ferror(fp) == 0);
      if (fclose(fp) != 0) {
        ok = false;
      }
      if (!ok) {
        fprintf(stderr, "Failed to write %s\n", filename.c_str());
      }
      return ok;
    }

    const std::string& getName() const{
      return name;
    }

    // Line in the loaded file; 0 for the root or if not loaded from a file.
    uint32_t getLine() const{
      return line;
    }

    const std::vector<AttributePtr>& getAttributes() const{
      return attributes;
    }

    const std::vector<ConfigNodePtr>& getChildren() const{
      return children;
    }

    ConfigNodePtr findChildByName(const std::string& childName) const{
      ConfigNodePtr result = nullptr;
      for (const auto& child : children) {
        if (child->getName().compare(childName) == 0) {
          if(result != nullptr) {
            fprintf(stderr, "[WARNING] Multiple children with name %s found. Returning the first one.\n", childName.c_str());
            return result;
          }
          result =  child;
        }
      }
      return result;
    }

    std::vector<ConfigNodePtr> findChildrenByName(const std::string& childName) const{
      std::vector<ConfigNodePtr> result;
      for (const auto& child : children) {
        if (child->getName().compare(childName) == 0) {
          result.push_back(child);
        }
      }
      return result;
    }

    AttributePtr findAttributeByKey(const std::string& key) const{
      for (const auto& attr : attributes) {
        if (attr->getKey().compare(key) == 0) {
          return attr;
        }
      }
      return nullptr;
    }

    ~ConfigNode() = default;

    void addAttribute(AttributePtr attr){
      attributes.push_back(attr);
    }

    void addChild(ConfigNodePtr child){
      children.push_back(child);
    }

    ConfigNode(std::string name) : name(name) {}
    ConfigNode() : name("") {}

  private:
    std::string name;
    uint32_t line = 0;
    std::vector<AttributePtr> attributes;
    std::vector<ConfigNodePtr> children;
};

#endif /* CONFIG_NODE_H */
