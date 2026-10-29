//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_HOST_UTILS_H_
#define PTI_UTILS_HOST_UTILS_H_

#include <inttypes.h>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else /* _WIN32 */
#include <unistd.h>
#endif /* _WIN32 */

static inline std::string GetHostName(void) {
  char hname[256];
#ifdef _WIN32
  static_assert(256 >= MAX_COMPUTERNAME_LENGTH + 1, "Buffer too small to fit hostname");
  DWORD size = sizeof(hname);
  GetComputerNameA(hname, &size);
#else  /* _WIN32 */
  gethostname(hname, sizeof(hname));
#endif /* _WIN32 */
  hname[255] = 0;
  return hname;
}

// Convert pointer/handle values to hex string
template <typename T>
inline std::string ToHexString(T val) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "0x%" PRIx64, (uintptr_t)val);
  return std::string(buffer);
}

// The length of the valid UTF-8 sequence that starts at s[i]: 1 for ASCII, 2 to 4
// for a multibyte character; 0 if the bytes there are not valid UTF-8 (a stray
// continuation byte, a sequence cut short, an overlong form, a surrogate, or a code
// point above U+10FFFF).
inline size_t Utf8SequenceLength(const std::string& s, size_t i) {
  const unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    return 1;
  }
  size_t n = 0;
  uint32_t cp = 0;
  if (c >= 0xc2 && c <= 0xdf) { n = 2; cp = c & 0x1f; }
  else if (c >= 0xe0 && c <= 0xef) { n = 3; cp = c & 0x0f; }
  else if (c >= 0xf0 && c <= 0xf4) { n = 4; cp = c & 0x07; }
  else { return 0; }
  if (i + n > s.size()) {
    return 0;
  }
  for (size_t k = 1; k < n; ++k) {
    const unsigned char d = static_cast<unsigned char>(s[i + k]);
    if ((d & 0xc0) != 0x80) {
      return 0;
    }
    cp = (cp << 6) | (d & 0x3f);
  }
  const bool overlong = (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000);
  if (overlong || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) {
    return 0;
  }
  return n;
}

// text as the contents of a JSON string (RFC 8259): quote and backslash escaped,
// control characters as \b \f \n \r \t or \u00XX. Valid UTF-8 is kept as it is;
// each byte that is not valid UTF-8 (a name cut inside a character, a lone 0xff)
// becomes U+FFFD, so the result is always valid UTF-8 JSON.
//   text  any bytes: a kernel, ITT or device name, a command-line argument, ...
// Returns the escaped text, without surrounding quotes.
inline std::string JsonEscape(const std::string& text) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c >= 0x80) {
      const size_t n = Utf8SequenceLength(text, i);
      if (n == 0) {
        out += "\\ufffd";
      } else {
        out.append(text, i, n);
        i += n - 1;
      }
      continue;
    }
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 0xf];
        } else {
          out += ch;
        }
    }
  }
  return out;
}

// text as a complete JSON string: escaped by JsonEscape() and quoted.
//   text  any bytes
// Returns "<escaped text>", so names from the application or the driver cannot
// break the trace or the config log.
inline std::string JsonString(const std::string& text) {
  return "\"" + JsonEscape(text) + "\"";
}

// true if every character of s is an ASCII decimal digit; also true for "", which
// callers rule out themselves.
inline bool IsDigits(const std::string& s) {
  return std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) ==
         s.end();
}

// Parses s as an int.
//   s    the text; std::stoi() rules apply (leading spaces and a sign allowed)
//   out  receives the value; untouched on failure
// Returns false if s is not a number, has anything after it, or does not fit an
// int. Never throws, unlike the std::stoi() it wraps.
inline bool ToInt(const std::string& s, int& out) {
  try {
    size_t used = 0;
    const int value = std::stoi(s, &used);
    if (used != s.size()) {
      return false;
    }
    out = value;
    return true;
  } catch (...) {
    return false;
  }
}

// Parses s as a uint32_t.
//   s    the text; decimal digits only, no sign or spaces
//   out  receives the value; untouched on failure
// Returns false if s is empty, not decimal digits, or larger than UINT32_MAX.
// Leading zeros are allowed ("007" is 7), as std::stoul allowed them.
inline bool ToUint32(const std::string& s, uint32_t& out) {
  if (s.empty() || !IsDigits(s)) {
    return false;
  }
  // At most 10 significant digits fit; checked before strtoull, which saturates.
  const size_t first = std::min(s.find_first_not_of('0'), s.size() - 1);
  if (s.size() - first > 10) {
    return false;
  }
  const unsigned long long value = std::strtoull(s.c_str() + first, nullptr, 10);
  if (value > UINT32_MAX) {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

// Parses UNITRACE_ChromeEventBufferSize (--chrome-event-buffer-size).
//   value  the variable's value
//   out    receives the size in events: -1 (unbounded) if value is empty or "-1"
// Returns false, with out set to -1, if value is neither -1 nor a non-negative
// number that fits an int. unitrace rejects such a value; the collector, which
// cannot report it, falls back to unbounded.
inline bool ParseChromeEventBufferSize(const std::string& value, int& out) {
  out = -1;
  if (value.empty() || value == "-1") {
    return true;
  }
  int size = 0;
  if (!IsDigits(value) || !ToInt(value, size)) {
    return false;
  }
  out = size;
  return true;
}

#endif // PTI_UTILS_HOST_UTILS_H_
