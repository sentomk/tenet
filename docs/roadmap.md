# Roadmap

A living list of candidate components, roughly in priority order. Anything
here is a proposal, not a promise -- entries graduate to real work only when
a design note exists and the scope fits tenet's foundation-library charter:
small, dependency-free, C++20, correctness-first with friendly diagnostics.

## Near term

### ~~unique_resource~~ shipped

Implemented (see `include/tenet/resource/unique_resource.hpp`,
`docs/design/unique_resource.md`): `(acquire, release)` pair wrapped into a
move-only RAII handle, with `make_unique_resource_checked` for
sentinel-based acquirers and the scope family's exception baseline. Design
notes kept below for the record:

Modern take on P0053 (`std::experimental::unique_resource`): wrap an
`(acquire, release)` pair into an RAII handle.

```cpp
auto fd = tenet::unique_resource{open(path), close};
```

Natural sibling of the scope guard family -- reuses the same solved problems
(move-only semantics, exception baselines, noexcept boundaries, release()).
Pairs with TENET_DEFER: defer for anonymous cleanup, unique_resource for
named resources.

### TENET_ASSERT

Runtime counterpart to our compile-time philosophy (static_assert messages,
concept constraints): assert macros with a configurable failure handler,
controllable via build settings rather than only NDEBUG.

Needed as groundwork before expected lands anyway.

### expected

Flagship gap on C++20: no std::expected until C++23. Two open questions to
settle first:

- Error model: plain template parameter `E`, or a lightweight error-code /
  error-domain facility alongside?
- Showcase synergy: expected + scope_fail gives transaction-style rollback;
  README examples should demonstrate the combination.

## Mid term

### unordered_dense_map

Dense hash map: `vector<entry>` storage plus a sparse index array
(`ankerl::unordered_dense` style). Contiguous iteration, insertion order
preserved, serializable layout -- several times faster than
std::unordered_map for typical workloads, at manageable implementation
complexity.

API improvements over std regardless of internals:

- Heterogeneous lookup enabled by default (std requires transparent-hash
  boilerplate almost nobody writes)
- Iterator/pointer stability rules documented explicitly -- dropping node
  stability is precisely where the speed comes from
- No extract/node-handle machinery (it exists in std only to serve
  stability guarantees we don't make)

A Swiss-table-style flat_hash_map may follow later as a second
implementation behind the same interface, driven by profiling needs, not
before.

### slot_map

Generational handle map (aka handle map / generational arena), proposed to
SG14 but never standardized; a staple of game engines:

```cpp
tenet::slot_map<Enemy> enemies;
auto h = enemies.emplace(...);   // stable handle: slot + generation
enemies.erase(h);                // stale handles fail safely by generation
```

Solves "stored a pointer/iterator, container mutated, everything dangles".
Structurally a cousin of sparse sets (both are dense + sparse two-layer
indexes); differentiates on handle semantics and erase strategy. Fits the
library's ownership-first character and combines well with
unique_resource / scope guards for resource-pool patterns.

### function_ref

Non-owning callable view (std gets one in C++26). Small, self-contained,
and a good vehicle for growing our invocable-concept vocabulary next to
ScopeGuardAction.

## Backlog / ideas

- **narrow_cast / narrow** -- checked narrowing conversions (GSL-style);
  days of work, standard foundation fare.
- **small_vector** -- inline capacity N, spills to heap beyond that.
  EASTL/folly staple; static_vector arrives in C++26 but the spill-to-heap
  strategy remains unserved.
- **ring_buffer** -- fixed-capacity circular buffer (boost::circular_buffer
  style) with overwrite and reject policies.
- **String utilities**, formatting glue, coroutine support -- revisit once
  the core above has stabilized.
