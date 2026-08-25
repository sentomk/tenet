#include "tenet/scope/scope_success.hpp"

#include <stdexcept>
#include <type_traits>

#include "gtest/gtest.h"

namespace {

TEST(ScopeSuccessTest, RunsOnNormalExit) {
  bool ran = false;
  {
    auto guard = tenet::scope_success{[&] { ran = true; }};
    EXPECT_FALSE(ran);
  }
  EXPECT_TRUE(ran);
}

TEST(ScopeSuccessTest, DoesNotRunOnEscapingException) {
  bool ran = false;
  try {
    auto guard = tenet::scope_success{[&] { ran = true; }};
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
  }
  EXPECT_FALSE(ran);
}

TEST(ScopeSuccessTest, ReleaseCancelsAction) {
  bool ran = false;
  {
    auto guard = tenet::scope_success{[&] { ran = true; }};
    (void)guard.release();
  }
  EXPECT_FALSE(ran);
}

TEST(ScopeSuccessTest, ConstructedInCatchBlockCountsAsNormalExit) {
  // A guard created while handling an exception has a snapshot that already
  // includes it, so expiring inside the handler -- without letting a new
  // exception escape -- still counts as success.
  bool ran = false;
  int caught = 0;
  try {
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
    auto guard = tenet::scope_success{[&] { ran = true; }};
    ++caught;
  }
  EXPECT_EQ(caught, 1);
  EXPECT_TRUE(ran);
}

TEST(ScopeSuccessTest, MoveTransfersOwnership) {
  std::vector<int> log;
  auto make_guard = [&] { return tenet::scope_success{[&] { log.push_back(1); }}; };
  {
    auto sink = make_guard();
    EXPECT_TRUE(log.empty());
  }
  EXPECT_EQ(log, std::vector<int>{1});
}

TEST(ScopeSuccessTest, ExplicitMoveKeepsExceptionBaseline) {
  // Mirror of the scope_fail test: forcing the move constructor must keep
  // the uncaught-exception snapshot. A guard created during handling that
  // exits the catch block without a new escaping exception must fire.
  bool ran = false;
  int caught = 0;
  try {
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
    auto src = tenet::scope_success{[&] { ran = true; }};
    auto dst = std::move(src);  // NOLINT: exercised, not an elision
    ++caught;
  }
  EXPECT_EQ(caught, 1);
  EXPECT_TRUE(ran);
}

// Compile-time contract checks shared by the scope family. Note: these
// instantiate the class template, so F must satisfy its requirements too.
static_assert(!std::is_copy_constructible_v<tenet::scope_success<void (*)()>>);
static_assert(std::is_nothrow_move_constructible_v<tenet::scope_success<bool (*)()>>);

}  // namespace
