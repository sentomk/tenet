# Design note: scope

Status: implemented
Language baseline: C++20
References: P0053 (scope guards), C++23 `<scope>`

## Overview

The scope module attaches a nullary cleanup action to a lexical scope and
runs it according to how that scope exits. It is the general form of RAII for
cleanup that does not justify a dedicated owner type.

The module has three named guards and one convenience syntax:

| Component | Action runs when |
|---|---|
| `scope_exit` | the scope exits for any reason |
| `scope_fail` | a new exception is escaping the scope |
| `scope_success` | the uncaught-exception count is unchanged |
| `TENET_DEFER` | the scope exits for any reason; implemented with `scope_exit` |

The mental model is an armed, move-only pair of a callable and an exit
condition. Destruction evaluates the condition and invokes the callable at
most once. `release()` disarms the guard and returns the callable; move
construction transfers the armed state.

## Goals

- Cover unconditional cleanup, rollback-on-failure, and commit-on-success.
- Preserve exactly-once ownership of the pending action across moves.
- Keep the common use site small through class template argument deduction
  and `TENET_DEFER`.
- Work on C++20 while keeping behavior close to the standard scope-guard
  family.
- Diagnose unsuitable action types at the guard boundary rather than through
  errors deep in a destructor instantiation.

## Non-goals

- No copy semantics: two live owners could invoke the same action twice.
- No move assignment: replacing a live pending action introduces ordering and
  exception-safety questions without a compelling scope-guard use case.
- No dynamic allocation, type erasure, or allocator support; the callable is
  stored directly in the guard.
- No exception transport from cleanup. Actions run in destructors and must not
  throw.
- `TENET_DEFER` has no cancellation syntax. Code that needs cancellation or a
  meaningful guard name should use `scope_exit` directly.

## Shared guard contract

The three guards expose the same ownership surface:

```cpp
template <typename F>
class scope_exit { // scope_fail and scope_success have the same shape
public:
  explicit scope_exit(F action);
  scope_exit(scope_exit&& other);
  scope_exit(const scope_exit&) = delete;
  scope_exit& operator=(const scope_exit&) = delete;
  scope_exit& operator=(scope_exit&&) = delete;
  ~scope_exit();

  [[nodiscard]] F release();
};
```

`F` is stored by value. Constructors take it by value so a lambda or function
object has one obvious ownership-transfer path into the guard. Deduction
guides allow the normal spelling:

```cpp
auto cleanup = tenet::scope_exit{[&] { close(fd); }};
```

Every guard has an `active_` bit. Its state transitions are:

| Operation | Target state | Source state |
|---|---|---|
| construction | armed | not applicable |
| move construction | inherits source state | disarmed |
| `release()` | disarmed | not applicable |
| destruction | action may run, then lifetime ends | not applicable |

The source is disarmed only after its callable has moved successfully. If the
move constructor of `F` throws, the source remains responsible for the
pending action. `release()` deliberately disarms before returning the moved
callable; users that require a compiler-checked non-throwing transfer should
use a nothrow-move-constructible action.

### Construction failure baseline

A guard constructor uses a function-try-block around member initialization.
If moving the by-value parameter into storage throws, no guard exists to run
the action later, so the constructor invokes the parameter immediately and
rethrows. Cleanup therefore still happens once on this failure path.

This does not change the destructor boundary: if the action itself throws,
the exception escapes a `noexcept` destructor and the process terminates.
Cleanup actions are required by contract to be non-throwing.

## `scope_exit`

`scope_exit` is the primitive guard. Its destructor runs the stored action
whenever the guard is still armed, independent of normal return, early
return, or exception unwinding.

```cpp
FILE* file = std::fopen(path, "r");
auto close_file = tenet::scope_exit{[&] { std::fclose(file); }};
```

Use it for local cleanup where the exit reason is irrelevant. Named resource
ownership that needs `get()`, `reset()`, or handle transfer belongs in
`unique_resource` instead.

## `scope_fail`

`scope_fail` provides rollback semantics. At construction it stores
`std::uncaught_exceptions()`. At destruction it runs the action only when the
current count is greater than that snapshot:

```cpp
active_ && std::uncaught_exceptions() > escapes_
```

Comparing against a snapshot matters. An exception thrown and caught entirely
inside the guarded scope does not count as that scope failing. A guard created
inside a `catch` block also inherits the already-active exception as its
baseline; it fires only if a new exception starts escaping.

```cpp
auto tx = db.begin();
auto rollback = tenet::scope_fail{[&] { tx.rollback(); }};
tx.update(...);
tx.commit();
```

The exception-count snapshot moves with the action. Recomputing it in the
move target could reclassify the same exit path and run or suppress rollback
incorrectly.

## `scope_success`

`scope_success` is the mirror of `scope_fail`. It takes the same
`std::uncaught_exceptions()` snapshot and runs only when the count is unchanged
at destruction:

```cpp
active_ && std::uncaught_exceptions() == escapes_
```

"Success" therefore means that no new exception is escaping past the guard's
scope. A guard constructed and destroyed normally inside a `catch` block is a
success even though another exception is being handled around it.

```cpp
auto publish = tenet::scope_success{[&] { notify_observers(); }};
mutate_state();
```

For ordinary lexical use, `scope_fail` and `scope_success` are complementary:
a greater exception count selects failure and an unchanged count selects
success. Moving a guard so that it outlives an exception already active at
construction can make the count decrease; the equality check therefore
suppresses `scope_success` in that non-lexical case.

## `TENET_DEFER`

`TENET_DEFER` is syntax sugar for an anonymous `scope_exit` with reference
capture:

```cpp
TENET_DEFER { close(fd); };
```

Conceptually, the expansion is:

```cpp
auto tenet_defer_guard_<line> = ::tenet::defer + [&]() { close(fd); };
```

The `defer` tag and `operator+` factory are needed because the macro must end
with a lambda introducer that the caller's `{ ... }` can complete. The factory
returns an armed `scope_exit`, so defer inherits its ownership and exception
semantics instead of implementing another guard.

The generated variable includes `__LINE__`. This keeps the macro expansion's
token sequence stable when used in inline functions or templates in headers.
Two defers on the same source line are intentionally rejected by a variable
redefinition. Separate declarations in one scope are destroyed in reverse
construction order, giving defer its expected LIFO behavior.

Reference capture makes the current values of surrounding locals visible when
the action eventually runs. It also means normal lambda lifetime rules apply:
references used by the action must remain valid until the guard is destroyed.

## Concepts and diagnostics

The public concepts live in `tenet::concepts`:

- `ScopeGuardAction<F>` requires a move-constructible `F` invocable as `F&()`.
  Invocation through a non-const lvalue matches how the destructor owns and
  calls the stored object.
- `NothrowScopeGuardAction<F>` adds a statically provable non-throwing
  invocation requirement.

The guards accept `ScopeGuardAction` rather than requiring the stricter
concept because ordinary lambdas are not implicitly `noexcept`. Each class
also has a plain-language `static_assert` so direct misuse names the violated
contract. Generic code whose cleanup must be compiler-checked can constrain
its own API with `NothrowScopeGuardAction`.

## Header layout

- `include/tenet/scope.hpp` — aggregate header for the complete family.
- `include/tenet/scope/scope_exit.hpp` — unconditional primitive.
- `include/tenet/scope/scope_fail.hpp` — failure-conditioned guard.
- `include/tenet/scope/scope_success.hpp` — success-conditioned guard.
- `include/tenet/scope/defer.hpp` — `defer` tag and `TENET_DEFER` macro.
- `include/tenet/concepts/scope_concepts.hpp` — public action concepts.

## Testing focus

Tests cover each exit mode (fall-through, early return, escaping exception,
and internally caught exception), cancellation through `release()`, ownership
transfer and moved-from destruction, construction with a throwing move,
exception snapshots created inside handlers, defer LIFO order and reference
capture, and concept acceptance/rejection. Compile-time checks also keep the
family move-only and preserve conditional `noexcept` on move construction.
