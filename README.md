# ToySPSC

Toy lock-free single-producer / single-consumer (SPSC) ring buffer written in C++23.

> 日本語版は [`README.ja.md`](./README.ja.md)

At the moment the source tree only ships a `Hello, ToySPSC` main — the purpose of
this repository is to host the surrounding developer tooling so that the SPSC
implementation can land into a ready-made build, test, bench, profile and CI loop.

## Requirements

The only host-side requirements are:

- [`podman`](https://podman.io/) (on macOS: run `podman machine start` once)
- [`just`](https://github.com/casey/just)

Everything else — `cmake`, `ninja`, `gcc-14`, `clang-tools`, `lcov`, `doxygen`,
`perf`, `valgrind` — lives inside the `toyspsc:dev` Linux container described by
`Containerfile` (Nix-based). You never install a C++ toolchain on the host.

## Quickstart

```sh
just image      # build the toyspsc:dev container image
just test       # configure + build + ctest
just bench      # Google Benchmark microbenchmarks
just profile    # perf stat + perf record + perf report --stdio
just fmt        # clang-format -i
just fmt-check  # clang-format --dry-run --Werror
just lint       # clang-tidy
just clean      # remove build-* and perf.data
```

`just profile` launches the container with
`--cap-add SYS_ADMIN --security-opt seccomp=unconfined` so `perf` can read
hardware counters.

## Layout

```
.
├── CMakeLists.txt       # C++23, FetchContent GoogleTest/Google Benchmark
├── Containerfile        # nixos/nix base, nix profile install toolchain
├── justfile             # host-side Podman task runner
├── Doxyfile             # API docs config
├── .clang-format        # Google style, 100 col
├── .clang-tidy          # bugprone/cert/cppcore/modernize/...
├── .github/workflows/   # GitHub Actions CI (test / fmt / lint / bench-smoke)
├── src/
│   ├── main.cc          # hello world placeholder
│   └── spsc/            # future home of the SPSC headers
├── test/
│   └── sanity_test.cc   # GoogleTest placeholder
└── bench/
    └── sanity_bench.cc  # Google Benchmark placeholder
```

## Acknowledgements

The tooling layout follows [diohabara/mini-oems](https://github.com/diohabara/mini-oems).
