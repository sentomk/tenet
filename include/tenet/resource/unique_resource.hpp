#pragma once

// tenet::unique_resource -- see the class comment below.

#include <type_traits>
#include <utility>

#include "tenet/concepts/resource_concepts.hpp"

namespace tenet {

// An RAII handle owning one resource through an (acquire, release) pair.
//
// unique_resource stores a resource handle R (an int fd, FILE*, socket, raw
// pointer, ...) together with its deleter D, and invokes the deleter on the
// resource exactly once when the guard is destroyed -- unless ownership was
// given away first via release() or reset().
//
// Example:
//
//   auto fd = tenet::unique_resource{open(path, flags), close};
//   // ... work with fd.get(); closed automatically at scope exit.
//
//   // APIs whose acquire returns an invalid sentinel instead of throwing:
//   auto f = tenet::make_unique_resource_checked(
//       std::fopen(path, "r"), static_cast<FILE*>(nullptr), &std::fclose);
//
// The natural sibling of the scope-guard family: defer/scope_exit attach
// anonymous cleanup to a scope; unique_resource gives a *named* resource a
// handle that can be queried, passed around by move, released early, or
// swapped for another resource via reset(). It reuses the same solved
// problems -- move-only semantics, exception baselines, noexcept
// boundaries.
//
// Exception contract:
// - Moving is noexcept when R and D are nothrow-move-constructible (true for
//   raw handles and function pointers). If a move nevertheless throws, the
//   source's resource is still deleted in the handler: it never leaks.
// - The destructor invokes the deleter unconditionally-if-armed; destructors
//   are implicitly noexcept, so an exception escaping the deleter calls
//   std::terminate. Keep deleters non-throwing; use
//   concepts::NothrowUniqueResourceDeleter to make that compiler-checked.
template <typename R, typename D>
  requires concepts::UniqueResourceHandle<R> && concepts::UniqueResourceDeleter<D, R>
class unique_resource {
  // Friendly misuse diagnostics: fail fast in the class body with plain
  // language instead of deep template instantiation errors at the call site.
  // The requirements are the public concepts in
  // tenet/concepts/resource_concepts.hpp.
  static_assert(concepts::UniqueResourceHandle<R>,
                "tenet::unique_resource<R, D>: R must be a move-constructible "
                "non-array object type serving as the resource handle, e.g. "
                "int, FILE*, or a pointer into some registry.");
  static_assert(concepts::UniqueResourceDeleter<D, R>,
                "tenet::unique_resource<R, D>: D must be a move-constructible "
                "callable invocable as 'D&(R&)', e.g. a lambda taking the "
                "handle '[](int fd) { close(fd); }' or 'void close(int&)'.");

public:
  // Takes ownership of r and arranges d(r) on destruction unless released,
  // reset, or moved away. explicit prevents an (acquire, release) pair from
  // silently converting into a guard; use CTAD:
  // 'tenet::unique_resource{open(p), close}'.
  //
  // Noexcept when both moves are; if a move throws anyway, the constructor
  // cannot exist -- so the deleter runs on r immediately in the handler and
  // the exception propagates: the resource never leaks.
  explicit unique_resource(R&& r, D&& d) noexcept(std::is_nothrow_move_constructible_v<R> &&
                                                  std::is_nothrow_move_constructible_v<D>) try
      : res_(std::move(r)), del_(std::move(d)) {
  } catch (...) {
    std::move(d)(r);
  }

  // Lvalue variants: acquiring already produced a named handle/deleter.
  explicit unique_resource(const R& r,
                           const D& d) noexcept(std::is_nothrow_copy_constructible_v<R> &&
                                                std::is_nothrow_copy_constructible_v<D>)
      : res_(r), del_(d) {}
  explicit unique_resource(R&& r, const D& d) noexcept(std::is_nothrow_move_constructible_v<R> &&
                                                       std::is_nothrow_copy_constructible_v<D>)
      : res_(std::move(r)), del_(d) {}
  explicit unique_resource(const R& r, D&& d) noexcept(std::is_nothrow_copy_constructible_v<R> &&
                                                       std::is_nothrow_move_constructible_v<D>)
      : res_(r), del_(std::move(d)) {}

  // Transfers ownership from another guard: the target becomes armed with
  // the resource and deleter, the source becomes disarmed and holds nothing.
  //
  // Noexcept when both member moves are. If a move throws anyway, the source
  // still holds everything it started with, so the handler deletes the
  // resource there -- same exactly-once baseline as scope_exit's move ctor.
  unique_resource(unique_resource&& other) noexcept(std::is_nothrow_move_constructible_v<R> &&
                                                    std::is_nothrow_move_constructible_v<D>) try
      : res_(std::move(other.res_)),
        del_(std::move(other.del_)),
        armed_(std::exchange(other.armed_, false)) {
  } catch (...) {
    other.del_(other.res_);
    other.armed_ = false;
    throw;
  }

  // Replaces the owned resource: deletes what this guard currently owns,
  // then takes over other's. Only available when R and D are nothrow
  // move-assignable -- after deleting our old resource, a throwing member
  // move cannot be rolled back safely, so we don't offer the operation we
  // can't specify crisply (see docs/design/resource.md#unique_resource).
  unique_resource& operator=(unique_resource&& other) noexcept(
      std::is_nothrow_move_assignable_v<R> && std::is_nothrow_move_assignable_v<D> &&
      noexcept(std::declval<D&>()(std::declval<R&>())))
    requires(std::is_nothrow_move_assignable_v<R> && std::is_nothrow_move_assignable_v<D>)
  {
    reset();
    res_ = std::move(other.res_);
    del_ = std::move(other.del_);
    armed_ = std::exchange(other.armed_, false);
    return *this;
  }

  // Copying would let two guards delete the same resource.
  unique_resource(const unique_resource&) = delete;
  unique_resource& operator=(const unique_resource&) = delete;

  // Invokes the deleter on the held resource iff armed, exactly once.
  // Destructors are implicitly noexcept, so an exception thrown by the
  // deleter reaches std::terminate -- see the class comment above.
  ~unique_resource() { reset(); }

  // Disarms the guard and returns the resource: the caller takes over the
  // responsibility to clean up. Typical use: hand a descriptor to an API
  // that adopts it.
  [[nodiscard]] R release() noexcept(std::is_nothrow_move_constructible_v<R>) {
    armed_ = false;
    return std::move(res_);
  }

  // Deletes the owned resource now (if any) and leaves the guard empty.
  void reset() noexcept(concepts::NothrowUniqueResourceDeleter<D, R>) {
    if (armed_) {
      armed_ = false;
      del_(res_);
    }
  }

  // Deletes the old resource, then takes ownership of r. If the deleter
  // throws while deleting the old resource, r is destroyed normally by the
  // caller's expression and the exception propagates; the guard stays empty.
  void reset(R r) noexcept(concepts::NothrowUniqueResourceDeleter<D, R> &&
                           std::is_nothrow_move_assignable_v<R>) {
    reset();
    res_ = std::move(r);
    armed_ = true;
  }

  // Access to the held resource. get() never disarms anything.
  [[nodiscard]] const R& get() const noexcept { return res_; }

  // Access to the deleter, e.g. to invoke it manually on a released handle.
  [[nodiscard]] D& get_deleter() noexcept { return del_; }
  [[nodiscard]] const D& get_deleter() const noexcept { return del_; }

  // Pointer sugar: dereferencing acts directly on the held handle. Only
  // available for pointer-shaped resources (FILE*, sockets, pimpl pointers),
  // so integer handles like fds don't get surprising dereference syntax.
  //
  // Implemented as constrained member templates rather than plain members
  // with requires-clauses: instantiating the class template instantiates
  // every member *declaration*, and a trailing return type like
  // 'decltype(*declval<R&>())' would be eagerly substituted -- and fail --
  // even for non-pointer R. A member function template's declaration, by
  // contrast, is only instantiated when actually used.
  template <typename P = R>
    requires std::is_pointer_v<P>
  [[nodiscard]] auto operator*() const noexcept -> decltype(*std::declval<P&>()) {
    return *res_;
  }

  template <typename P = R>
    requires std::is_pointer_v<P>
  [[nodiscard]] P operator->() const noexcept {
    return res_;
  }

private:
  R res_;
  D del_;
  // True while this guard still owns the resource. Cleared by release(),
  // reset(), or when the guard is moved from, so the deleter fires exactly
  // once per acquired resource.
  bool armed_ = true;
};

// Deduction guides: let unique_resource{acquire_result, closer} deduce R and
// D, since lambdas cannot be named explicitly.
template <typename R, typename D>
unique_resource(R&&, D&&) -> unique_resource<std::remove_cvref_t<R>, std::remove_cvref_t<D>>;

// Factory mirroring construction; handy where CTAD is awkward (e.g. naming
// the type in a return statement of generic code).
template <typename R, typename D>
[[nodiscard]] auto make_unique_resource(R&& r, D&& d) noexcept(
    std::is_nothrow_constructible_v<unique_resource<std::remove_cvref_t<R>, std::remove_cvref_t<D>>,
                                    R, D>) {
  return unique_resource<std::remove_cvref_t<R>, std::remove_cvref_t<D>>(std::forward<R>(r),
                                                                         std::forward<D>(d));
}

// Factory for acquirers that report failure with an invalid sentinel (-1 for
// fds, nullptr for FILE*) instead of throwing: the returned guard is
// disarmed when r == invalid, so an invalid handle passes through untouched
// instead of being "closed". Requires r and invalid to be comparable.
template <typename R, typename D, typename S>
  requires std::equality_comparable_with<S, R>
[[nodiscard]] auto make_unique_resource_checked(R&& r, const S& invalid, D&& d) noexcept(
    std::is_nothrow_constructible_v<unique_resource<std::remove_cvref_t<R>, std::remove_cvref_t<D>>,
                                    R, D> &&
    noexcept(std::declval<const R&>() == invalid) &&
    std::is_nothrow_move_constructible_v<std::remove_cvref_t<R>>) {
  using UR = unique_resource<std::remove_cvref_t<R>, std::remove_cvref_t<D>>;
  UR res(std::forward<R>(r), std::forward<D>(d));
  if (res.get() == invalid) {
    (void)res.release();
  }
  return res;
}

}  // namespace tenet
