#include "tenet/scope/scope_fail.hpp"

#include <stdexcept>
#include <type_traits>

#include "gtest/gtest.h"

namespace {

TEST(ScopeFailTest, NormalExitDoesNotRunAction) {
  bool ran = false;
  {
    auto guard = tenet::scope_fail{[&] { ran = true; }};
    EXPECT_FALSE(ran);
  }
  EXPECT_FALSE(ran);
}

TEST(ScopeFailTest, RunsOnEscapingException) {
  bool ran = false;
  try {
    auto guard = tenet::scope_fail{[&] { ran = true; }};
    EXPECT_FALSE(ran);
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
  }
  EXPECT_TRUE(ran);  // action ran during unwinding
}

TEST(ScopeFailTest, CaughtInsideScopeDoesNotRunAction) {
  bool ran = false;
  {
    auto guard = tenet::scope_fail{[&] { ran = true; }};
    try {
      throw std::runtime_error("handled inside");
    } catch (const std::runtime_error&) {
      // handled within the scope: not a failure of this scope
    }
    EXPECT_FALSE(ran);
  }
  EXPECT_FALSE(ran);
}

TEST(ScopeFailTest, ReleaseCancelsAction) {
  bool ran = false;
  try {
    auto guard = tenet::scope_fail{[&] { ran = true; }};
    (void)guard.release();
    throw 42;
  } catch (int) {
  }
  EXPECT_FALSE(ran);
}

TEST(ScopeFailTest, MoveTransfersOwnership) {
  bool ran = false;
  auto make_guard = [&] { return tenet::scope_fail{[&] { ran = true; }}; };
  try {
    auto sink = make_guard();  // ownership moves into the returned guard
    EXPECT_FALSE(ran);
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
  }
  EXPECT_TRUE(ran);
}

TEST(ScopeFailTest, ExplicitMoveKeepsExceptionBaseline) {
  // Force the move constructor (copy elision would hide it): create the
  // guard while an exception is active, move it, then exit normally. The
  // baseline snapshot must travel with the move -- an uninitialized one
  // would misclassify this normal exit as failure.
  bool ran = false;
  try {
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
    auto src = tenet::scope_fail{[&] { ran = true; }};
    auto dst = std::move(src);  // NOLINT: exercised, not an elision
  }
  EXPECT_FALSE(ran);
}

// Compile-time contract checks shared by the scope family. Note: these
// instantiate the class template, so F must satisfy its requirements too.
static_assert(!std::is_copy_constructible_v<tenet::scope_fail<void (*)()>>);
static_assert(std::is_nothrow_move_constructible_v<tenet::scope_fail<bool (*)()>>);

}  // namespace
