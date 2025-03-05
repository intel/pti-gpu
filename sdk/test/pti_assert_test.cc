#include "pti_assert.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(PtiAssertTest, CheckCorrectAssertion) {
  const int value_1 = 0;
  const int value_2 = 0;
  EXPECT_EQ(value_1, value_2);
  PTI_ASSERT(value_1 == value_2);
}

TEST(PtiAssertTest, CheckIncorrectAssertion) {
  const int value_1 = 0;
  const int value_2 = 1;
  EXPECT_NE(value_1, value_2);
  // clang-format off
  EXPECT_DEATH_IF_SUPPORTED(PTI_ASSERT(value_1 == value_2), ::testing::HasSubstr(__FILE__ ":"));
  // clang-format on
  PTI_ASSERT(value_1 != value_2);
}
