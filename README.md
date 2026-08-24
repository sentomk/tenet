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

### Compiled mode

Configure with `-DTENET_HEADER_ONLY=OFF` and link `tenet::tenet` as usual.

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

Note that clang-format cannot rewrite quote styles; the CI check is the
authority here.

## License

MIT. See [LICENSE](LICENSE).
