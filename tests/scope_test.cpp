
#include "tenet/scope.hpp"

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace {

TEST(ScopeExitTest, RunsActionOnDestruction) {
  bool ran = false;
  {
    auto guard = tenet::scope_exit{[&] { ran = true; }};
    EXPECT_FALSE(ran);
  }
  EXPECT_TRUE(ran);
}

TEST(ScopeExitTest, RunsActionOnEarlyReturn) {
  bool ran = false;
  [&] {
    auto guard = tenet::scope_exit{[&] { ran = true; }};
    return;  // guard must fire on this exit path
  }();
  EXPECT_TRUE(ran);
}

TEST(ScopeExitTest, ReleaseCancelsActionAndReturnsCallable) {
  int calls = 0;
  {
    auto guard = tenet::scope_exit{[&] { ++calls; }};
    auto fn = guard.release();
    EXPECT_EQ(calls, 0);
    // The action is handed back and can be invoked manually.
    fn();
    EXPECT_EQ(calls, 1);
  }
  // Disarmed: the destructor must not run it a second time.
  EXPECT_EQ(calls, 1);
}

TEST(ScopeExitTest, MoveTransfersOwnership) {
  std::vector<int> log;
  auto make_guard = [&] { return tenet::scope_exit{[&] { log.push_back(42); }}; };
  {
    // Ownership moves into sink via the returned guard.
    auto sink = make_guard();
    EXPECT_TRUE(log.empty());
  }
  EXPECT_EQ(log, std::vector<int>{42});
}

TEST(ScopeExitTest, MovedFromGuardRunsNothing) {
  int calls = 0;
  auto first = tenet::scope_exit{[&] { ++calls; }};
  {
    auto second = std::move(first);  // NOLINT -- ownership moves to second
    EXPECT_EQ(calls, 0);
  }
  EXPECT_EQ(calls, 1);  // second fired once on expiry
}                       // first is a moved-from guard: destroying it here must not run again

// A callable whose move constructor throws, to exercise the constructor's
// failure path.
struct ThrowingMoveAction {
  int* calls;
  explicit ThrowingMoveAction(int* c) : calls(c) {}
  ThrowingMoveAction(const ThrowingMoveAction& other) : calls(other.calls) {}
  ThrowingMoveAction(ThrowingMoveAction&&) { throw std::runtime_error("boom"); }
  void operator()() const { ++*calls; }
};

TEST(ScopeExitTest, ActionRunsEvenIfConstructorThrows) {
  int calls = 0;
  // Construction cannot complete (the move into the guard throws), but the
  // action must still run exactly once before the exception propagates.
  EXPECT_THROW({ auto guard = tenet::scope_exit{ThrowingMoveAction{&calls}}; }, std::runtime_error);
  EXPECT_EQ(calls, 1);
}
}  // namespace
