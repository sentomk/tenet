#pragma once

// Requirements of the scope guard components, published as reusable
// concepts. These are public API: user code can reference them for
// documentation purposes or in its own generic constraints, e.g.
//
//   template <tenet::concepts::ScopeGuardAction F>
//   void install_cleanup(F&& action);

#include <concepts>
#include <type_traits>

namespace tenet::concepts {

// A callable suitable for tenet::scope_exit / tenet::scope_fail /
// tenet::scope_success: it must be move-constructible (the guard stores it by
// value and supports transferring ownership via move) and invocable with no
// arguments through a non-const lvalue (the guard's destructor calls it
// without arguments).
template <typename F>
concept ScopeGuardAction = std::move_constructible<F> && std::invocable<F&>;

// A ScopeGuardAction whose invocation is additionally declared noexcept.
//
// A guard runs its action from a destructor, which is implicitly noexcept --
// an escaping exception would call std::terminate. Ordinary lambdas do not
// carry noexcept, so the guards themselves accept plain ScopeGuardAction and
// document that hazard instead. Use this stricter concept when a cleanup must
// provably never terminate the process (crash handlers, logging, safety-
// critical code): satisfying it makes the guarantee compiler-checked rather
// than convention-based.
template <typename F>
concept NothrowScopeGuardAction = ScopeGuardAction<F> && std::is_nothrow_invocable_v<F&>;

}  // namespace tenet::concepts
