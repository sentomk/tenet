# Tenet

A modern C++ foundation library. Work in progress.

## Requirements

- C++20 capable compiler (GCC 11+, Clang 14+, MSVC 19.29+)
- CMake 3.21+

## Usage

### Header-only (default)

```cmake
add_subdirectory(tenet)
target_link_libraries(your_target PRIVATE tenet::tenet)
```

```cpp
#include <tenet/tenet.hpp>
```

(Angle brackets here reflect consuming an installed copy of the library;
code inside the tenet repo itself uses quoted includes -- see
[Style](#style).)

### Compiled mode

Configure with `-DTENET_HEADER_ONLY=OFF` and link `tenet::tenet` as usual.

## Components

### Scope guards

Cleanup actions that run automatically when a scope exits:

| Component | Runs when |
|---|---|
| `tenet::scope_exit` | always (normal exit, early return, exception) |
| `tenet::scope_fail` | only if an exception is unwinding past the scope |
| `tenet::scope_success` | only on normal exit |

```cpp
#include <tenet/scope.hpp>

void transfer(Account& from, Account& to, int amount) {
    auto tx = db.begin();
    tenet::scope_fail rollback{[&] { tx.rollback(); }};  // failure safety net
    debit(from, amount);
    credit(to, amount);
    tx.commit();   // reached only if nothing threw
}
```

Guards are move-only; `release()` disarms a guard and hands back the action.
The stored callable must not throw.

### TENET_DEFER

Go-style defer for unconditional, anonymous cleanup -- sugar over
`scope_exit`, so the action sits right where you think of it and no guard
variable needs a name:

```cpp
#include <tenet/scope/defer.hpp>

FILE* f = fopen("data.txt", "r");
TENET_DEFER { fclose(f); };
TENET_DEFER { log("leaving scope"); };   // multiple defers run in LIFO order
```

Use a named `scope_fail`/`scope_success` instead when the cleanup is
conditional or the variable name carries meaning (`rollback`, `commit`, ...).

### Concepts

Requirements of library components are published as reusable concepts under
`include/tenet/concepts/`, grouped by module and namespaced accordingly
(e.g. `tenet::concepts::ScopeGuardAction` in `<tenet/concepts/scope_concepts.hpp>`).
Misuses are diagnosed with plain-language `static_assert` messages.

## Development

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Style

Include style (enforced by CI):

```cpp
#include <type_traits>      // standard library: angle brackets
#include "tenet/tenet.hpp"  // project headers: quotes
#include "gtest/gtest.h"    // third-party headers: quotes
```

Note that clang-format cannot rewrite quote styles; the include-style CI
check is the authority there. Formatting itself is also enforced: CI runs
`clang-format --dry-run --Werror` over `include/`, `src/`, and `tests/`
before building.

## License

MIT. See [LICENSE](LICENSE).
