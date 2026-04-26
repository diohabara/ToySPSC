# CLAUDE.md

Guidance for Claude Code when working inside this repository.

## Language standard

- **C++23 is mandatory.** `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 23` with
  `CXX_STANDARD_REQUIRED ON` and `CXX_EXTENSIONS OFF`. Do not add code that
  would not compile under C++23.
- Write idiomatic C++23, not C++11/14/17 translated to a modern standard.
  If a newer facility exists, prefer it over its older counterpart.

## Prefer the C++23 standard library over legacy patterns

| Prefer                               | Over                                  |
| ------------------------------------ | ------------------------------------- |
| `#include <print>`, `std::println`   | `#include <iostream>`, `std::cout`    |
| `std::format`, `std::formatted_size` | `snprintf`, `std::ostringstream`      |
| `std::expected<T, E>`                | error-code out-params, sentinel ints (for recoverable errors) |
| `std::optional<T>`                   | sentinel values, bool + out-param (for "value may not exist" semantics) |
| `std::span<T>`, `std::mdspan`        | `T*` + length, raw 2D arrays          |
| `std::string_view`                   | `const char*` + length, `const std::string&` for read-only params |
| `std::ranges::*` / views             | hand-rolled index loops over `std::vector` |
| `std::array<T, N>`                   | C arrays                              |
| `std::bit_cast`                      | `reinterpret_cast` for type punning   |
| `std::source_location`               | `__FILE__` / `__LINE__` macros        |
| `constexpr` / `consteval`            | runtime constants, macros             |
| `[[nodiscard]]` on value-returning APIs | silent discard                     |
| Trailing return type `auto f() -> T` | only when it improves readability; fine to use either style consistently |
| Concepts + `requires`                | SFINAE, `enable_if`                   |
| `std::jthread`, `std::stop_token`    | `std::thread` + manual join           |
| `std::atomic_ref`                    | ad-hoc `reinterpret_cast` to atomic   |
| `std::int32_t`, `std::uint64_t` etc. | bare `int`, `unsigned long` (use fixed-width types from `<cstdint>`) |

## SPSC-specific guidance (for when the implementation lands)

- Use `std::atomic<T>` with explicit `memory_order` arguments. Avoid
  `memory_order_seq_cst` unless you can justify it — for a single-producer /
  single-consumer ring buffer, `acquire`/`release` pairs are usually what you
  want.
- Pad / align hot indices to `std::hardware_destructive_interference_size` to
  avoid false sharing between producer and consumer.
- The public header should live under `src/spsc/` and be header-only so the
  `toyspsc_lib` INTERFACE target can expose it.

## Things to avoid

- `using namespace std;` at file scope.
- Raw `new` / `delete` (use RAII, `std::unique_ptr`, containers).
- C-style casts. Use `static_cast`, `const_cast`, `reinterpret_cast`,
  `std::bit_cast`.
- `NULL`, `0` for pointer literals. Use `nullptr`.
- `typedef`. Use `using` aliases.
- Uncaught exceptions in `noexcept` code paths.
- Raw `for` / `while` loops. Use `std::ranges` algorithms, `std::views`,
  `std::ranges::for_each` etc. Write functional style over imperative loops.
- `import std;`. GCC 14's named-module support for the standard library is
  still experimental and requires opting into `CMAKE_EXPERIMENTAL_CXX_MODULE_*`.
  We stay on header includes until the toolchain stabilises — revisit when we
  move to GCC 15+ and CMake ≥ 3.30 with stable `import std` support.

## Build / test workflow

Every developer command runs inside the `toyspsc:dev` container via the
host-side `justfile`. Do not install `cmake`, `gcc`, or `clang-tools` on the
host — use the container.

```sh
just image      # build toyspsc:dev
just test       # cmake + ctest
just bench      # Google Benchmark
just profile    # perf stat/report/annotate + flamegraph.svg + cachegrind
just lint       # clang-tidy
just fmt-check  # clang-format --dry-run --Werror
```

Before claiming any change is complete:

1. `just fmt` (auto-format) then `just fmt-check` (verify clean).
2. `just lint` — clang-tidy must be green.
3. `just test` — all ctest targets must pass.
4. If the change touches SPSC hot paths, also run `just bench` and
   `just profile` to sanity-check cache-miss / branch-miss counters.
5. If the change affects implementation behaviour or design, update the
   corresponding documentation under `docs/` to keep code and docs in sync.
