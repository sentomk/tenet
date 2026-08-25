#pragma once

// tenet::TENET_DEFER -- see the macro comment below.

// Registers a cleanup action that runs when the enclosing scope exits --
// Go-style defer, built on tenet::scope_exit.
//
//   FILE* f = fopen("data.txt", "r");
//   TENET_DEFER { fclose(f); };      // runs at every exit path from here
//   TENET_DEFER { log("leaving"); }; // multiple defers run in LIFO order
//
// The action sees the enclosing scope by reference ([&] capture), so local
// variables are usable directly. Defers cannot be cancelled or moved -- if
// conditional cancellation matters, use a named tenet::scope_exit instead.
//
// Implementation notes:
// - The generated guard variable embeds __LINE__, which expands identically
//   in every translation unit -- so a defer inside a header's inline function
//   or template produces the same token sequence everywhere (ODR-safe). Two
//   defers on the same source line would collide, but fail loudly with a
//   redefinition error rather than silently misbehaving.
// - The macro must end in a form the caller's '{ ... }' can close directly
//   (there is no trailing ')'), hence the 'tenet::defer + [&]()' factory
//   shape below -- the same trick folly's SCOPE_EXIT uses. The resulting
//   nullary lambda satisfies tenet::ScopeGuardAction, so misuse diagnostics
//   apply unchanged.

#include <type_traits>
#include <utility>

#include "tenet/concepts/scope_concepts.hpp"
#include "tenet/scope/scope_exit.hpp"

namespace tenet {

namespace detail {

// Type of the tenet::defer tag. A named type (rather than an anonymous
// struct) so that decltype(tenet::defer) denotes the same type in every
// translation unit.
struct defer_t {};

// Found via ADL on defer_t when users write 'tenet::defer + action'.
template <typename F>
  requires concepts::ScopeGuardAction<std::remove_cvref_t<F>>
[[nodiscard]] constexpr auto operator+(defer_t, F&& action) {
  return scope_exit<std::remove_cvref_t<F>>(std::forward<F>(action));
}

}  // namespace detail

// Tag used by TENET_DEFER: 'defer + action' wraps the action in an anonymous,
// armed tenet::scope_exit. Usable without the macro:
//
//   auto g = tenet::defer + [&] { cleanup(); };
inline constexpr detail::defer_t defer{};

}  // namespace tenet

#define TENET_DEFER_TOKEN_PASTE_IMPL(a, b) a##b
#define TENET_DEFER_TOKEN_PASTE(a, b) TENET_DEFER_TOKEN_PASTE_IMPL(a, b)

#define TENET_DEFER_GUARD_NAME TENET_DEFER_TOKEN_PASTE(tenet_defer_guard_, __LINE__)

#define TENET_DEFER auto TENET_DEFER_GUARD_NAME = ::tenet::defer + [&]()
