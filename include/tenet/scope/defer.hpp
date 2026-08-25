#pragma once

// tenet::TENET_DEFER -- see the macro comment below.

// Registers a cleanup action that runs when the enclosing scope exits --
// Go-style defer, built on tenet::scope_exit.
//
//   FILE* f = fopen("data.txt", "r");
//   TENET_DEFER { fclose(f); };      // runs at every exit path from here
//   TENET_DEFER { log("leaving"); }  // multiple defers run in LIFO order
//
// The action sees the enclosing scope by reference ([&] capture), so local
// variables are usable directly. Defers cannot be cancelled or moved -- if
// conditional cancellation matters, use a named tenet::scope_exit instead.
//
// Implementation notes:
// - The generated guard variable embeds __COUNTER__ (falling back to
//   __LINE__), so multiple defers in one scope never collide.
// - The macro must end in a form the caller's '{ ... }' can close directly
//   (there is no trailing ')'), hence the 'tenet::defer + [&]()' factory
//   shape below -- the same trick folly's SCOPE_EXIT uses. The resulting
//   nullary lambda satisfies tenet::ScopeGuardAction, so misuse diagnostics
//   apply unchanged.

#include "tenet/concepts/scope_concepts.hpp"
#include "tenet/scope/scope_exit.hpp"

#include <type_traits>
#include <utility>

namespace tenet {

// Tag used by TENET_DEFER: 'defer + action' wraps the action in an anonymous,
// armed tenet::scope_exit. Not useful on its own.
inline constexpr struct {
} defer;

template <typename F>
  requires concepts::ScopeGuardAction<std::remove_cvref_t<F>>
[[nodiscard]] constexpr auto operator+(decltype(defer), F&& action) {
  return scope_exit<std::remove_cvref_t<F>>(std::forward<F>(action));
}

}  // namespace tenet

#define TENET_DEFER_TOKEN_PASTE_IMPL(a, b) a##b
#define TENET_DEFER_TOKEN_PASTE(a, b) TENET_DEFER_TOKEN_PASTE_IMPL(a, b)

#if defined(__COUNTER__)
#define TENET_DEFER_GUARD_NAME \
  TENET_DEFER_TOKEN_PASTE(tenet_defer_guard_, __COUNTER__)
#else
#define TENET_DEFER_GUARD_NAME \
  TENET_DEFER_TOKEN_PASTE(tenet_defer_guard_, __LINE__)
#endif

#define TENET_DEFER auto TENET_DEFER_GUARD_NAME = ::tenet::defer + [&]()
