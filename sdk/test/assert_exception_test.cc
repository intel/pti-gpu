#include <gtest/gtest.h>

#include "view_helpers.h"

class AssertExceptionFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override { ViewStateInstance().SetState(pti_result::PTI_SUCCESS); }

  void TearDown() override {}
};

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUFValid) {
  uint64_t aTS = ApplyTimeShift(10, -10);
  ASSERT_EQ(aTS, 0ULL);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUnderflow) {
  uint64_t aTS = ApplyTimeShift(0, -1);
  ASSERT_EQ(aTS, 0ULL);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOFValid) {
  uint64_t aTS = ApplyTimeShift(UINT64_MAX - 1, 1);
  ASSERT_EQ(aTS, UINT64_MAX);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOverflow) {
  uint64_t aTS = ApplyTimeShift(UINT64_MAX, 1);
  ASSERT_EQ(aTS, 0ULL);
}
