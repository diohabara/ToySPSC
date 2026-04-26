# ToySPSC

Toy lock-free single-producer / single-consumer (SPSC) ring buffer written in C++23.

> 日本語版は [`README.ja.md`](./README.ja.md)

The source tree currently includes a header-only `SpscQueue<T, Capacity>`, a
small producer/consumer demo, GoogleTest coverage, and Google Benchmark
microbenchmarks. The queue is fixed-capacity, power-of-two sized, and intended
for one producer thread and one consumer thread.

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
│   ├── main.cc          # small SPSC producer/consumer demo
│   └── spsc/            # header-only SPSC queue implementation
├── test/
│   ├── sanity_test.cc
│   └── spsc_queue_test.cc
└── bench/
    ├── sanity_bench.cc
    └── spsc_queue_bench.cc
```
