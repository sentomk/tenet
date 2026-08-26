// Tests for tenet::unique_resource.

#include "tenet/resource/unique_resource.hpp"

#include <stdexcept>
#include <utility>

#include "gtest/gtest.h"

namespace {

// A handle that lets tests count how many times it was "closed", so we can
// assert exactly-once deletion without relying on real OS resources.
struct CountingHandle {
  int id = 0;
  int* close_count = nullptr;

  bool operator==(const CountingHandle&) const = default;
};

// Deleter shaped like real closers: takes the mutable handle (deleters may
// overwrite it), bumps a counter.
constexpr auto counting_closer = [](CountingHandle& h) {
  *h.close_count += 1;
  h.id = -1;  // defense in depth: invalidate the stored handle
};

TEST(UniqueResourceTest, DestructorRunsDeleterExactlyOnce) {
  int closes = 0;
  {
    CountingHandle h{7, &closes};
    auto g = tenet::unique_resource{h, counting_closer};
    EXPECT_EQ(closes, 0);
  }
  EXPECT_EQ(closes, 1);
}

TEST(UniqueResourceTest, GetReturnsHeldResource) {
  int closes = 0;
  CountingHandle h{42, &closes};
  const auto g = tenet::unique_resource{std::as_const(h), counting_closer};
  EXPECT_EQ(g.get().id, 42);
  // Guard still armed afterwards: destruction closes once.
  EXPECT_EQ(g.get().id, 42);
}

TEST(UniqueResourceTest, ReleaseDisarmsAndReturnsResource) {
  int closes = 0;
  auto g = tenet::unique_resource{CountingHandle{3, &closes}, counting_closer};
  [[maybe_unused]] CountingHandle taken = g.release();
  // Disarmed guard destroys silently; ownership now belongs to the caller.
  EXPECT_EQ(closes, 0);
}

TEST(UniqueResourceTest, MoveTransfersOwnershipToTargetOnly) {
  int closes = 0;
  {
    auto source = tenet::unique_resource{CountingHandle{9, &closes}, counting_closer};
    auto target = std::move(source);
    EXPECT_EQ(target.get().id, 9);
    EXPECT_EQ(closes, 0);
  }
  // Only the target's destructor had something to delete.
  EXPECT_EQ(closes, 1);
}

TEST(UniqueResourceTest, ResetDeletesNowThenGuardIsReusable) {
  int closes = 0;
  auto g = tenet::unique_resource{CountingHandle{1, &closes}, counting_closer};
  g.reset();
  EXPECT_EQ(closes, 1);
  // Empty reset() is a no-op.
  g.reset();
  EXPECT_EQ(closes, 1);

  // The guard can adopt another resource afterwards.
  g.reset(CountingHandle{2, &closes});
  EXPECT_EQ(g.get().id, 2);
}

TEST(UniqueResourceTest, ResetReplacesOldResourceWithNewOne) {
  int closes = 0;
  {
    auto g = tenet::unique_resource{CountingHandle{1, &closes}, counting_closer};
    g.reset(CountingHandle{2, &closes});
    EXPECT_EQ(closes, 1);  // old resource deleted at swap time
    EXPECT_EQ(g.get().id, 2);
  }
  EXPECT_EQ(closes, 2);  // replacement deleted at scope exit
}

TEST(UniqueResourceTest, GetDeleterExposesStoredCloser) {
  int closes = 0;
  CountingHandle other{5, &closes};
  {
    auto g = tenet::unique_resource{CountingHandle{1, &closes}, counting_closer};
    g.get_deleter()(other);  // manual invocation on any handle of type R
    EXPECT_EQ(closes, 1);
    EXPECT_EQ(other.id, -1);
  }
}

TEST(UniqueResourceTest, PointerSugarDereferencesHeldPointer) {
  struct Widget {
    int value = 123;
    int* closes = nullptr;
    ~Widget() { *closes += 1; }
  };
  int closes = 0;
  {
    auto closer = [](Widget* w) { delete w; };
    auto g = tenet::unique_resource{new Widget{.value = 123, .closes = &closes}, closer};
    EXPECT_EQ((*g).value, 123);
    EXPECT_EQ(g->value, 123);
  }
  EXPECT_EQ(closes, 1);
}

TEST(UniqueResourceTest, CheckedFactoryPassesInvalidSentinelThrough) {
  int closes = 0;

  // Simulate an acquiring API that reports failure with an invalid sentinel.
  const auto acquire = [&](bool fail) -> CountingHandle {
    return fail ? CountingHandle{-1, nullptr} : CountingHandle{11, &closes};
  };
  constexpr CountingHandle invalid{-1, nullptr};

  {
    auto good = tenet::make_unique_resource_checked(acquire(false), invalid, counting_closer);
    EXPECT_EQ(good.get().id, 11);
  }
  EXPECT_EQ(closes, 1);

  {
    auto bad = tenet::make_unique_resource_checked(acquire(true), invalid, counting_closer);
  }
  EXPECT_EQ(closes, 1);  // invalid sentinel was never closed
}

TEST(UniqueResourceTest, MakeFactoryMatchesConstruction) {
  int closes = 0;
  {
    auto g = tenet::make_unique_resource(CountingHandle{8, &closes}, counting_closer);
    EXPECT_EQ(g.get().id, 8);
  }
  EXPECT_EQ(closes, 1);
}

// --- exception baselines -----------------------------------------------------

template <typename T>
struct ThrowingMove {
  T value{};
  explicit ThrowingMove(T v) : value(std::move(v)) {}
  ThrowingMove(const ThrowingMove&) noexcept = default;
  ThrowingMove(ThrowingMove&&) noexcept(false) { throw std::runtime_error{"move"}; }
  ThrowingMove& operator=(const ThrowingMove&) = default;
  ThrowingMove& operator=(ThrowingMove&&) noexcept(false) { return *this; }
};

TEST(UniqueResourceTest, MoveCtorExceptionBaselineDeletesSourceResource) {
  int closes = 0;
  auto closer = [&closes](ThrowingMove<int>& h) {
    closes += 1;
    h.value = -1;
  };
  // Copy-construct the guard (ThrowingMove's move ctor always throws, so the
  // lvalue overload is the only way to build one).
  ThrowingMove<int> raw{4};
  auto source = tenet::unique_resource{raw, closer};

  // The move constructor throws while moving members; its handler must still
  // delete the source's resource instead of leaking it.
  try {
    [[maybe_unused]] auto doomed = std::move(source);
    FAIL() << "move construction should have thrown";
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(closes, 1);
}

TEST(UniqueResourceTest, MoveAssignmentRequiresNothrowAssignableMembers) {
  static_assert(
      std::is_move_assignable_v<tenet::unique_resource<int, decltype([](int& fd) { fd = -1; })>>);
  static_assert(!std::is_move_assignable_v<
                tenet::unique_resource<ThrowingMove<int>, decltype([](ThrowingMove<int>&) {})>>);
}

TEST(UniqueResourceTest, ConceptsAcceptHandlesAndRejectBadOnes) {
  using namespace tenet::concepts;
  static_assert(UniqueResourceHandle<int>);
  static_assert(UniqueResourceHandle<void*>);
  static_assert(!UniqueResourceHandle<int[4]>);
  static_assert(!UniqueResourceHandle<std::string&>);

  static_assert(UniqueResourceDeleter<decltype(counting_closer), CountingHandle>);
  static_assert(NothrowUniqueResourceDeleter<decltype([](int& fd) noexcept { fd = -1; }), int>);
  static_assert(!UniqueResourceDeleter<int (*)(int, char), int>);  // wrong signature
  static_assert(!UniqueResourceDeleter<decltype(counting_closer), std::string[2]>);
}

}  // namespace
