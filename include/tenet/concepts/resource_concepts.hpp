#pragma once

// Requirements of the unique_resource component, published as reusable
// concepts. These are public API: user code can reference them for
// documentation purposes or in its own generic constraints, e.g.
//
//   template <tenet::concepts::UniqueResourceDeleter<D, R> D>
//   void register_closer(D closer);

#include <concepts>
#include <type_traits>

namespace tenet::concepts {

// A type usable as the resource handle of tenet::unique_resource: a plain
// non-array object type that can be moved (the guard stores it by value and
// supports transferring ownership via move). Raw handles -- int fds, FILE*,
// HWND, pointers into some registry -- all qualify.
template <typename R>
concept UniqueResourceHandle =
    std::move_constructible<R> && std::is_object_v<R> && !std::is_array_v<R>;

// A callable suitable as the deleter for handle type R: it must be
// move-constructible (stored by value next to the resource) and invocable
// through a non-const lvalue with a mutable lvalue reference to the handle,
// e.g. 'void close(int& fd)'. The mutable reference lets deleters overwrite
// the stored handle as defense in depth (e.g. set an fd to -1).
//
// Like scope-guard actions, a deleter must not throw when invoked at guard
// destruction time; an escaping exception reaches std::terminate. Prefer
// NothrowUniqueResourceDeleter where that hazard must be compiler-checked.
template <typename D, typename R>
concept UniqueResourceDeleter =
    std::move_constructible<D> && std::is_object_v<D> && std::invocable<D&, R&>;

// A UniqueResourceDeleter whose invocation is additionally declared
// noexcept. Use this stricter concept when cleanup must provably never
// terminate the process (crash handlers, logging, safety-critical code).
template <typename D, typename R>
concept NothrowUniqueResourceDeleter =
    UniqueResourceDeleter<D, R> && std::is_nothrow_invocable_v<D&, R&>;

}  // namespace tenet::concepts
