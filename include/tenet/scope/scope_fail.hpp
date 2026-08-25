#pragma once

// tenet::scope_fail -- see the class comment below.

#include <exception>
#include "tenet/concepts/scope_concepts.hpp"

#include <type_traits>
#include <utility>

namespace tenet {

// A scope guard that runs a callable only when the enclosing scope exits via
// an exception -- rollback semantics for transaction-style code.
//
// The guard snapshots the number of uncaught exceptions at construction; the
// action runs in the destructor only if more exceptions are in flight by
// then, i.e. an exception is unwinding past this scope. Normal exits (fall-
// off, early return) and exceptions caught inside the scope do not trigger
// it.
//
// Example:
//
//   auto tx = db.begin();
//   auto guard = tenet::scope_fail{[&] { tx.rollback(); }};
//   tx.exec("INSERT ...");  // on success the guard expires silently
//
// The action can be cancelled by calling release(); ownership can be
// transferred by move construction; copying is deleted.
//
// Note: the stored callable must not throw -- see tenet::scope_exit for why
// an escaping exception would call std::terminate.
template <typename F>
class scope_fail {
  // Friendly misuse diagnostics -- see tenet::scope_exit for rationale.
  static_assert(concepts::ScopeGuardAction<F>,
                "tenet::scope_fail<F>: F must be a move-constructible "
                "callable invocable with no arguments, e.g. a nullary lambda "
                "'[&] { ... }' or 'void (*)()'. The guard stores the callable "
                "by value and its destructor calls it without arguments.");

public:
  // Takes ownership of f and arms the guard. On a throwing move into the
  // member (which would leave the guard unable to exist), the action runs
  // immediately and the exception propagates -- same contract as
  // tenet::scope_exit.
  explicit scope_fail(F f) noexcept(std::is_nothrow_move_constructible_v<F>) try
      : fn_(std::move(f)), escapes_(std::uncaught_exceptions()) {
  } catch (...) {
    f();
  }

  // Transfers the pending action from another guard, including the
  // uncaught-exception baseline (the escape point moves with the action, not
  // with the source guard's destruction site). The source guard becomes
  // disarmed (its action will never fire), mirroring release().
  scope_fail(scope_fail&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
      : fn_(std::move(other.fn_)),
        escapes_(other.escapes_),
        active_(std::exchange(other.active_, false)) {}

  // Copying would allow the same cleanup to run twice.
  scope_fail(const scope_fail&) = delete;
  scope_fail& operator=(const scope_fail&) = delete;
  scope_fail& operator=(scope_fail&&) = delete;

  // Runs the stored action unless disarmed or moved away, and only when an
  // exception is unwinding past this scope. Comparing against the snapshot
  // taken at construction (rather than asking "is there an exception?") is
  // what distinguishes a genuinely escaping exception from one that was
  // thrown and caught again inside the scope.
  ~scope_fail() {
    if (active_ && std::uncaught_exceptions() > escapes_) {
      fn_();
    }
  }

  // Disarms the guard and hands back the stored callable.
  [[nodiscard]] F release() noexcept(std::is_nothrow_move_constructible_v<F>) {
    active_ = false;
    return std::move(fn_);
  }

private:
  F fn_;
  // Number of uncaught exceptions when the guard was created. If the guard is
  // itself constructed while handling an exception (e.g. inside a catch
  // block), the baseline already reflects that, so only a *new* escaping
  // exception triggers the action.
  const int escapes_;
  // True while this guard still owns the action. Cleared by release() or when
  // moved from, so the action fires exactly once.
  bool active_ = true;
};

// Deduction guide: lets scope_fail{callable} deduce F from the argument.
template <typename F>
scope_fail(F) -> scope_fail<F>;

}  // namespace tenet
