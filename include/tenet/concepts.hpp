#pragma once

// Concepts for tenet's components.
//
// Every reusable concept published by a tenet component lives here, so
// "where is that constraint defined?" always has the same answer. The
// concepts are public API: user code can reference them for documentation
// purposes or in its own generic constraints, e.g.
//
//   template <tenet::ScopeGuardAction F> void install_cleanup(F&& action);

#include <concepts>

namespace tenet {

// A callable suitable for tenet::scope_exit / tenet::scope_fail /
// tenet::scope_success: it must be move-constructible (the guard stores it by
// value and supports transferring ownership via move) and invocable with no
// arguments through a non-const lvalue (the guard's destructor calls it
// without arguments).
template <typename F>
concept ScopeGuardAction =
    std::move_constructible<F> && std::invocable<F&>;

}  // namespace tenet
