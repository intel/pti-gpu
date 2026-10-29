//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Minimal strict JSON (RFC 8259) validator, used by the fuzz harnesses as an
// oracle: output unitrace writes to a trace file must parse.

#ifndef PTI_TOOLS_UNITRACE_FUZZ_JSON_CHECK_H_
#define PTI_TOOLS_UNITRACE_FUZZ_JSON_CHECK_H_

#include <cctype>
#include <string>

namespace json_check {

// A recursive-descent parser over one string that only checks it: it builds no
// value. i_ is the read position; every method returns false at the first byte
// that cannot continue a valid document. Nesting is limited to 64 levels, so a
// harness input cannot overflow the stack.
class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}

  // true if the whole string is one JSON value, with optional whitespace around
  // it and nothing after it.
  bool Document() {
    Ws();
    if (!Value(0)) return false;
    Ws();
    return i_ == s_.size();
  }

 private:
  // Skips the four whitespace characters JSON allows.
  void Ws() {
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
  }
  // Consumes the literal w (true, false or null) if the input continues with it.
  bool Lit(const char* w) {
    size_t n = std::char_traits<char>::length(w);
    if (s_.compare(i_, n, w) != 0) return false;
    i_ += n;
    return true;
  }
  // One value of any kind, chosen by its first character; depth is the nesting
  // level of the enclosing objects and arrays.
  bool Value(int depth) {
    if (depth > 64 || i_ >= s_.size()) return false;
    char c = s_[i_];
    if (c == '{') return Object(depth);
    if (c == '[') return Array(depth);
    if (c == '"') return String();
    if (c == 't') return Lit("true");
    if (c == 'f') return Lit("false");
    if (c == 'n') return Lit("null");
    return Number();
  }
  // An object, from its '{': "key": value pairs separated by commas.
  bool Object(int depth) {
    ++i_;
    Ws();
    if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
    for (;;) {
      Ws();
      if (i_ >= s_.size() || s_[i_] != '"' || !String()) return false;
      Ws();
      if (i_ >= s_.size() || s_[i_] != ':') return false;
      ++i_;
      Ws();
      if (!Value(depth + 1)) return false;
      Ws();
      if (i_ >= s_.size()) return false;
      if (s_[i_] == ',') { ++i_; continue; }
      if (s_[i_] == '}') { ++i_; return true; }
      return false;
    }
  }
  // An array, from its '[': values separated by commas.
  bool Array(int depth) {
    ++i_;
    Ws();
    if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
    for (;;) {
      Ws();
      if (!Value(depth + 1)) return false;
      Ws();
      if (i_ >= s_.size()) return false;
      if (s_[i_] == ',') { ++i_; continue; }
      if (s_[i_] == ']') { ++i_; return true; }
      return false;
    }
  }
  // A string, from its opening quote: no raw control characters, only valid UTF-8
  // (Utf8()), and only the escapes JSON defines (\uXXXX with four hex digits).
  bool String() {
    ++i_;
    while (i_ < s_.size()) {
      unsigned char c = static_cast<unsigned char>(s_[i_]);
      if (c == '"') { ++i_; return true; }
      if (c < 0x20) return false;
      if (c >= 0x80) {
        if (!Utf8()) return false;
        continue;
      }
      if (c == '\\') {
        ++i_;
        if (i_ >= s_.size()) return false;
        char e = s_[i_];
        if (e == 'u') {
          for (int k = 0; k < 4; ++k) {
            ++i_;
            if (i_ >= s_.size() || !std::isxdigit(static_cast<unsigned char>(s_[i_]))) return false;
          }
        } else if (std::string("\"\\/bfnrt").find(e) == std::string::npos) {
          return false;
        }
      }
      ++i_;
    }
    return false;
  }
  // One UTF-8 encoded character, from its lead byte (RFC 3629): JSON text is UTF-8,
  // so raw bytes that are not (a lone 0xff, a cut sequence, an overlong form, a
  // surrogate, a code point above U+10FFFF) make the document invalid. Checked here
  // on its own, not with the escaping code under test.
  bool Utf8() {
    const unsigned char lead = static_cast<unsigned char>(s_[i_]);
    int more = 0;
    unsigned char lo = 0x80, hi = 0xbf;  // valid range of the second byte
    if (lead >= 0xc2 && lead <= 0xdf) { more = 1; }
    else if (lead == 0xe0) { more = 2; lo = 0xa0; }
    else if (lead == 0xed) { more = 2; hi = 0x9f; }
    else if (lead >= 0xe1 && lead <= 0xef) { more = 2; }
    else if (lead == 0xf0) { more = 3; lo = 0x90; }
    else if (lead == 0xf4) { more = 3; hi = 0x8f; }
    else if (lead >= 0xf1 && lead <= 0xf3) { more = 3; }
    else return false;
    for (int k = 1; k <= more; ++k) {
      if (i_ + k >= s_.size()) return false;
      const unsigned char b = static_cast<unsigned char>(s_[i_ + k]);
      if (k == 1 ? (b < lo || b > hi) : (b < 0x80 || b > 0xbf)) return false;
    }
    i_ += more + 1;
    return true;
  }
  // A number: optional minus, an integer part without leading zeros, then an
  // optional fraction and exponent.
  bool Number() {
    size_t start = i_;
    if (i_ < s_.size() && s_[i_] == '-') ++i_;
    if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) return false;
    if (s_[i_] == '0') {
      ++i_;
    } else {
      while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
    }
    if (i_ < s_.size() && s_[i_] == '.') {
      ++i_;
      if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) return false;
      while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
    }
    if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
      ++i_;
      if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
      if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) return false;
      while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
    }
    return i_ > start;
  }

  const std::string& s_;
  size_t i_ = 0;
};

// true if s is one complete, valid JSON document.
inline bool IsValid(const std::string& s) { return Parser(s).Document(); }

}  // namespace json_check

#endif  // PTI_TOOLS_UNITRACE_FUZZ_JSON_CHECK_H_
