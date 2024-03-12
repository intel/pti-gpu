#include <gtest/gtest.h>

#include <cstring>

#include "view_handler.h"

class AssertExceptionFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUFValid) {
  uint64_t aTS = ApplyTimeShift(10, -10);
  ASSERT_EQ(aTS, 0);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUnderflow) {
  ASSERT_DEATH({ ApplyTimeShift(0, -1); }, "ApplyTimeShift");
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOFValid) {
  uint64_t aTS = ApplyTimeShift(UINT64_MAX - 1, 1);
  ASSERT_EQ(aTS, UINT64_MAX);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOverflow) {
  ASSERT_DEATH({ ApplyTimeShift(UINT64_MAX, 1); }, "ApplyTimeShift");
}

TEST_F(AssertExceptionFixtureTest, ExceptionGetViewNameAndCallbackThrow) {
  EXPECT_THROW({ GetViewNameAndCallback(pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION); },
               std::out_of_range);
}
