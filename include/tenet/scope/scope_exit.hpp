#pragma once

// tenet::scope_exit -- see the class comment below.

#include "tenet/concepts/scope_concepts.hpp"

#include <type_traits>
#include <utility>

namespace tenet {

// A scope guard that runs a callable when it goes out of scope.
//
// scope_exit stores a callable and invokes it in its destructor, so cleanup
// code runs on every exit path from the enclosing scope: normal fall-off, an
// early return, or stack unwinding caused by an exception. It is the
// lightweight, general form of RAII -- use it to attach cleanup to any code,
// not just to types that already manage a resource.
//
// Example:
//
//   FILE* f = std::fopen("data.txt", "r");
//   auto guard = tenet::scope_exit{[&] { std::fclose(f); }};
//   // ... work with f; it is closed automatically when guard is destroyed.
//
// The action can be cancelled by calling release(), which returns the stored
// callable and disarms the guard; ownership can also be transferred by move
// construction, and a moved-from guard runs nothing.
//
// Note: the stored callable must not throw. The destructor is noexcept (as
// destructors are by default), so an exception escaping the callable would
// call std::terminate. Keep cleanup code non-throwing.
template <typename F>
class scope_exit {
  // Friendly misuse diagnostics: fail fast in the class body with plain
  // language instead of deep template instantiation errors at the call site
  // or inside the destructor. The requirement itself is the public concept
  // tenet::ScopeGuardAction (see tenet/concepts/scope_concepts.hpp).
  static_assert(concepts::ScopeGuardAction<F>,
                "tenet::scope_exit<F>: F must be a move-constructible "
                "callable invocable with no arguments, e.g. a nullary lambda "
                "'[&] { ... }' or 'void (*)()'. The guard stores the callable "
                "by value and its destructor calls it without arguments.");

public:
  // Takes ownership of f and runs it on scope exit unless released. explicit
  // prevents a callable from silently converting into a scope_exit.
  //
  // Noexcept when F is nothrow-move-constructible (true for lambdas capturing
  // by reference/value of movable types). If that move nevertheless throws,
  // the guard cannot exist -- so the action is invoked immediately in the
  // handler and the exception propagates: the cleanup still happens exactly
  // once, matching std::experimental::scope_exit.
  explicit scope_exit(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
      try : fn_(std::move(f)) {
  } catch (...) {
    f();
  }


  // Transfers the pending action from another guard. The source guard becomes
  // disarmed (its action will never fire), mirroring release().
  //
  // Noexcept when F is nothrow-move-constructible, which is true for lambdas
  // capturing by reference/value of movable types; otherwise an exception
  // during the move could leave both guards armed and run the action twice,
  // so we conservatively propagate the exception before disarming anything.
  scope_exit(scope_exit&& other) noexcept(
      std::is_nothrow_move_constructible_v<F>)
      : fn_(std::move(other.fn_)), active_(std::exchange(other.active_, false)) {}

  // Copying would allow the same cleanup to run on two guards' exits.
  scope_exit(const scope_exit&) = delete;
  scope_exit& operator=(const scope_exit&) = delete;
  scope_exit& operator=(scope_exit&&) = delete;

  // Runs the stored action unless it was released or moved away. Destructors
  // are implicitly noexcept, so an exception thrown by the callable reaches
  // std::terminate -- see the class comment above.
  ~scope_exit() {
    if (active_) {
      fn_();
    }
  }

  // Disarms the guard: the action will no longer run at scope exit. Returns
  // the stored callable so the caller can invoke it manually, inspect it, or
  // hand it to another guard.
  [[nodiscard]] F release() noexcept(std::is_nothrow_move_constructible_v<F>) {
    active_ = false;
    return std::move(fn_);
  }

private:
  F fn_;
  // True while this guard still owns the action. Cleared by release() or when
  // the guard is moved from, so the action fires exactly once.
  bool active_ = true;
};

// Deduction guide: lets scope_exit{callable} deduce F from the argument, since
// a lambda's type cannot be named explicitly.
template <typename F>
scope_exit(F) -> scope_exit<F>;

}  // namespace tenet
