#include "gtest/gtest.h"

#include <stdexcept>
#include <type_traits>

#include "tenet/scope/scope_fail.hpp"

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

// Compile-time contract checks shared by the scope family. Note: these
// instantiate the class template, so F must satisfy its requirements too.
static_assert(!std::is_copy_constructible_v<tenet::scope_fail<void (*)()>>);
static_assert(
    std::is_nothrow_move_constructible_v<tenet::scope_fail<bool (*)()>>);

}  // namespace
