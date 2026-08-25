#include "gtest/gtest.h"

#include <stdexcept>
#include <string>
#include <type_traits>

#include "tenet/concepts/scope_concepts.hpp"

namespace {

TEST(ScopeGuardConceptsTest, NullaryLambdaIsAScopeGuardAction) {
    int x = 0;
    EXPECT_TRUE(tenet::concepts::ScopeGuardAction<decltype([&] { ++x; })>);
}

TEST(ScopeGuardConceptsTest, NonInvocableTypeIsNotAScopeGuardAction) {
    // Not callable with no arguments.
    EXPECT_FALSE(tenet::concepts::ScopeGuardAction<int>);
}

TEST(ScopeGuardConceptsTest,
     NonMovableCallableIsNotAScopeGuardAction) {
    struct Immovable {
        Immovable() = default;
        Immovable(Immovable&&) = delete;
        void operator()() {}
    };
    EXPECT_FALSE(tenet::concepts::ScopeGuardAction<Immovable>);
}

TEST(ScopeGuardConceptsTest, PlainLambdaIsNotAutomaticallyNothrow) {
    auto plain = [] {};
    // Even a trivially-empty body: lambdas are never implicitly noexcept,
    // so the stricter concept demands an explicit noexcept.
    EXPECT_TRUE(tenet::concepts::ScopeGuardAction<decltype(plain)>);
    EXPECT_FALSE(tenet::concepts::NothrowScopeGuardAction<decltype(plain)>);
}

void safe_fn() noexcept {}

TEST(ScopeGuardConceptsTest, ExplicitlyNoexceptCallableSatisfiesBoth) {
    void (*fnptr)() noexcept = safe_fn;
    static_assert(noexcept(fnptr()));
    EXPECT_TRUE(tenet::concepts::ScopeGuardAction<decltype(fnptr)>);
    EXPECT_TRUE(tenet::concepts::NothrowScopeGuardAction<decltype(fnptr)>);

    auto safe = [&]() noexcept { (void)this; };
    EXPECT_TRUE(tenet::concepts::NothrowScopeGuardAction<decltype(safe)>);
}

TEST(ScopeGuardConceptsTest, ThrowingCallableIsOnlyAPlainAction) {
    // Declared potentially-throwing: usable by the guards, but excluded from
    // the nothrow guarantee.
    auto maybe_throws = [] { throw std::runtime_error("boom"); };
    static_assert(!noexcept(maybe_throws()));
    EXPECT_TRUE(tenet::concepts::ScopeGuardAction<decltype(maybe_throws)>);
    EXPECT_FALSE(tenet::concepts::NothrowScopeGuardAction<decltype(maybe_throws)>);
}

}  // namespace
