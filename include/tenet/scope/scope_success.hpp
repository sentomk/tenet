#pragma once

// tenet::scope_success -- see the class comment below.

#include <exception>
#include <type_traits>
#include <utility>

namespace tenet {

// A scope guard that runs a callable only when the enclosing scope exits
// normally (no exception is unwinding past it) -- commit-on-success
// semantics.
//
// The guard snapshots the number of uncaught exceptions at construction; the
// action runs in the destructor only if the count is unchanged, meaning the
// scope completed without an escaping exception.
//
// Example:
//
//   {
//     auto tx = db.begin();
//     auto guard = tenet::scope_success{[&] { tx.commit(); }};
//     tx.exec("INSERT ...");   // commit happens only if nothing above threw
//   }
//
// Note the mirror-image behaviour of tenet::scope_fail: exactly one of the
// two guards fires on any given exit path.
//
// The action can be cancelled by calling release(); ownership can be
// transferred by move construction; copying is deleted.
//
// Note: the stored callable must not throw -- see tenet::scope_exit for why
// an escaping exception would call std::terminate.
template <typename F>
class scope_success {
public:
  // Takes ownership of f and arms the guard. If the move into the member
  // throws, the guard cannot exist -- the action runs immediately and the
  // exception propagates, matching the other scope components.
  explicit scope_success(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
      try : fn_(std::move(f)), escapes_(std::uncaught_exceptions()) {
  } catch (...) {
    f();
  }

  // Transfers the pending action from another guard. The source guard becomes
  // disarmed (its action will never fire), mirroring release().
  scope_success(scope_success&& other) noexcept(
      std::is_nothrow_move_constructible_v<F>)
      : fn_(std::move(other.fn_)),
        active_(std::exchange(other.active_, false)) {}

  // Copying would allow the same cleanup to run twice.
  scope_success(const scope_success&) = delete;
  scope_success& operator=(const scope_success&) = delete;
  scope_success& operator=(scope_success&&) = delete;

  // Runs the stored action unless disarmed, moved away, or exiting via an
  // exception. When the guard was created inside an exception handler (catch
  // block or cleanup during unwinding), the snapshot already includes the
  // active exception, so completing without a *new* escaping exception still
  // counts as success.
  ~scope_success() {
    if (active_ && std::uncaught_exceptions() == escapes_) {
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
  // Number of uncaught exceptions when the guard was created; see the
  // destructor comment.
  const int escapes_;
  // True while this guard still owns the action. Cleared by release() or when
  // moved from, so the action fires exactly once.
  bool active_ = true;
};

// Deduction guide: lets scope_success{callable} deduce F from the argument.
template <typename F>
scope_success(F) -> scope_success<F>;

}  // namespace tenet
