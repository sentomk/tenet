# Design note: resource

The resource module contains ownership types for named resources whose
lifetime is represented by an acquire/release pair. Each ownership type is a
separate chapter so future resource policies can share one module-level design
record without being folded into `unique_resource`.

## `unique_resource`

Status: implemented (v1)
Roadmap entry: near term -- unique_resource
References: P0053 (`std::experimental::unique_resource`), C++20

### Problem

Scope guards attach anonymous cleanup to a scope. Named resources need more:
a handle that owns one resource for its whole lifetime, can be queried,
released early, and swapped for another. P0053 fills exactly this gap, but
never made it into C++23's `std::unique_resource` either (it stalled after
LWG review); every project rolls its own fd/handle wrappers instead.

Goal: `tenet::unique_resource{open(path), close}` -- acquire/release pair
wrapped into a move-only RAII handle.

### Non-goals (v1)

- No shared ownership (that is `shared_ptr` territory, or a later
  `shared_resource` if ever needed).
- No node/exchange machinery beyond `release()`/`reset()`; no comparisons
  against other guards (`operator==` between two unique_resources in P0053
  has unclear utility and complicates semantics).
- No allocator support; `D` is any callable, not necessarily stateless.

### API surface

```cpp
template <typename R, typename D>
  requires UniqueResourceHandle<R> && UniqueResourceDeleter<D, R>
class unique_resource {
  explicit unique_resource(R&& r, D&& d);   // + const-lvalue variants
  unique_resource(unique_resource&&) /* noexcept when moves are */;
  ~unique_resource();                        // invokes deleter if armed

  R        release();      // disarm, return the resource
  void     reset();        // delete now, stay empty
  void     reset(R r);     // delete old, take r
  const R& get() const;
  D&       get_deleter();

  // Only when R is a pointer type:
  auto operator*() -> decltype(*declval<R>());
  auto operator->() -> R;
};

// Factories (CTAD guides also work for plain construction):
template <typename R, typename D> make_unique_resource(R&& r, D&& d);
template <typename R, typename D, typename S>
  make_unique_resource_checked(R&& r, const S& invalid, D&& d);
```

Semantics decisions, aligned with the scope-guard family:

- **Exactly-once baseline.** The destructor invokes `d(r)` once, iff armed.
  `release()` disarms; moving arms the target and disarms the source.
- **Exception baseline in the move constructor** mirrors `scope_exit`: if
  moving `R` or `D` throws, the *source* resource is still deleted (its
  deleter runs in the catch clause), so the resource never leaks. An
  exception escaping from the deleter itself reaches `std::terminate`,
  same contract as the guards' actions.
- **Move assignment is constrained**, unlike P0053 which specifies
  complicated partial-success rules for throwing member moves. We allow
  move assignment only when `R` and `D` are nothrow-move-assignable --
  the overwhelmingly common case -- and delete it otherwise. Rationale:
  after `reset()` frees the old resource, a throwing member move cannot be
  rolled back without leaking or double-freeing; P0053's wording papers
  over this with unspecified states. Correctness-first says: don't offer
  the operation we can't specify crisply. Revisit only with a concrete
  use case for throwing-move handles.
- **`make_unique_resource_checked`** handles APIs whose "acquire" returns
  an invalid sentinel (-1 for fds, nullptr for pointers) rather than
  throwing: the returned guard is disarmed when `r == invalid`, so closing
  an invalid handle twice becomes impossible.
- **Deleter sees `R&`, not `const R&`** -- deleters commonly overwrite the
  handle (e.g. set it to `-1`) as defense in depth; P0053 does the same.
- **Pointer sugar**: `*`/`->` exist because the majority of real uses wrap
  pointer-shaped handles (FILE*, sockets, pimpl). Constrained away for
  non-pointer `R` so integer handles don't get surprising dereference
  syntax.

### Concepts

Published next to the scope-guard concepts, same rationale (misuse
diagnostics in plain language, reusable constraints):

- `UniqueResourceHandle<R>` -- move-constructible non-array object type.
- `UniqueResourceDeleter<D, R>` -- move-constructible, invocable as
  `D&(R&)`.
- `NothrowUniqueResourceDeleter<D, R>` -- additionally provably
  noexcept; use where termination on failure is unacceptable.

Violating the requirements fails fast in the class body with a
`static_assert` naming the concept, not deep instantiation spam.

### Layout

- `include/tenet/resource/unique_resource.hpp` -- component header.
- `include/tenet/concepts/resource_concepts.hpp` -- concepts above.
- `tests/unique_resource_test.cpp` -- gtest suite.

### Testing focus

Lifetime (destroy/release/move/reset each fire exactly once), exception
baseline on the throwing move path, checked factory with invalid sentinels,
deleter-mutates-handle pattern, pointer sugar, concept accept/reject.
