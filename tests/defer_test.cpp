#include "tenet/scope/defer.hpp"

#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"

namespace {

TEST(DeferTest, RunsActionAtScopeExit) {
  bool ran = false;
  {
    TENET_DEFER { ran = true; };
    EXPECT_FALSE(ran);
  }
  EXPECT_TRUE(ran);
}

TEST(DeferTest, MultipleDefersRunInLifoOrder) {
  std::vector<int> order;
  {
    TENET_DEFER { order.push_back(1); };
    TENET_DEFER { order.push_back(2); };
    TENET_DEFER { order.push_back(3); };
  }
  EXPECT_EQ(order, (std::vector<int>{3, 2, 1}));
}

TEST(DeferTest, CapturesLocalsByReference) {
  int value = 7;
  bool ran = false;
  {
    TENET_DEFER { ran = value == 7; };
    value = 42;
  }
  EXPECT_FALSE(ran);  // saw the mutated value: captured by reference
}

TEST(DeferTest, RunsOnEarlyReturn) {
  std::vector<int> log;
  [&] {
    TENET_DEFER { log.push_back(1); };
    return;
  }();
  EXPECT_EQ(log, std::vector<int>{1});
}

TEST(DeferTest, RunsDuringExceptionUnwinding) {
  std::vector<int> log;
  try {
    TENET_DEFER { log.push_back(1); };
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(log, std::vector<int>{1});
}

// The tag is public API on its own: the macro is pure sugar over
// 'tenet::defer + [&] { ... }', so that path needs coverage too.
TEST(DeferTest, TagOperatorPlusWorksWithoutTheMacro) {
  bool ran = false;
  {
    auto guard = tenet::defer + [&] { ran = true; };
    EXPECT_FALSE(ran);
  }
  EXPECT_TRUE(ran);
}

}  // namespace
