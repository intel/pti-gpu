//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef UTILS_H_
#define UTILS_H_

#include <random>

class RandomFloatGen {
public:
  inline static constexpr float kUpperBound = 10.0;
  inline static constexpr float kLowerBound = -1 * kUpperBound;

  RandomFloatGen() : mt_engine_(rand_num_dev_()) {
  }

  inline float Get() {
    return distribution_(mt_engine_);
  }

private:
  std::random_device rand_num_dev_{}; // defaults to /dev/urandom libstdc++
  std::uniform_real_distribution<float> distribution_{kLowerBound, kUpperBound};
  std::mt19937 mt_engine_;
};

// Don't want to change the sample too much, so just use std C++ random number
// generator because C's rand() is flagged by static code analysis.
// (https://en.cppreference.com/w/cpp/numeric/random)
inline auto& RandomFloatGenInstance() {
  static RandomFloatGen rand_float_gen = {};
  return rand_float_gen;
}

// generate rand float [-10.0, 10.0]
inline auto random_float() {
  return RandomFloatGenInstance().Get();
}

#endif
