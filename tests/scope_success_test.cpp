#include "gtest/gtest.h"

#include <stdexcept>
#include <type_traits>

#include "tenet/scope/scope_success.hpp"

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
    auto make_guard = [&] {
        return tenet::scope_success{[&] { log.push_back(1); }};
    };
    {
        auto sink = make_guard();
        EXPECT_TRUE(log.empty());
    }
    EXPECT_EQ(log, std::vector<int>{1});
}

// Compile-time contract checks shared by the scope family. Note: these
// instantiate the class template, so F must satisfy its requirements too.
static_assert(
    !std::is_copy_constructible_v<tenet::scope_success<void (*)()>>);
static_assert(
    std::is_nothrow_move_constructible_v<tenet::scope_success<bool (*)()>>);

}  // namespace
