# ToySPSC host-side task runner.
# Every recipe runs inside the `toyspsc:dev` Linux container via Podman.
# Host requirements: `just` and `podman` only.

image_tag := "toyspsc:dev"
run := "podman run --rm -v $PWD:/work -w /work " + image_tag + " bash -lc"
run_privileged := "podman run --rm --privileged --security-opt seccomp=unconfined -v $PWD:/work -w /work " + image_tag + " bash -lc"

default:
    @just --list

# Build the dev container image
image:
    podman build -t {{image_tag}} -f Containerfile .

# Configure + build + run tests
test:
    {{run}} 'cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build && ctest --test-dir build --output-on-failure'

# Run microbenchmarks
bench:
    {{run}} 'cmake -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DTOYSPSC_ENABLE_BENCHMARKS=ON && cmake --build build-bench && ./build-bench/bench/toyspsc_bench'

# Full profiling: perf stat/report/annotate/flamegraph + cachegrind
# perf_event_paranoid is a host-level sysctl — set it on the podman machine VM first.
profile:
    -podman machine ssh 'sudo sysctl -w kernel.perf_event_paranoid=-1 >/dev/null 2>&1' 2>/dev/null || true
    {{run_privileged}} '\
      set -e && \
      cmake -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOYSPSC_ENABLE_BENCHMARKS=ON && \
      cmake --build build-profile && \
      BIN=./build-profile/bench/toyspsc_bench && \
      \
      echo "=== perf stat (software events; HW PMU unavailable in podman VM) ===" && \
      perf stat -e task-clock,context-switches,cpu-migrations,page-faults,minor-faults,major-faults $BIN && \
      echo "" && \
      \
      echo "=== perf record (cpu-clock sampling at 999 Hz) ===" && \
      perf record -e cpu-clock -F 999 -g --call-graph=dwarf -o perf.data $BIN && \
      echo "" && \
      \
      echo "=== perf report (top functions) ===" && \
      perf report --stdio -i perf.data | head -n 100 && \
      echo "" && \
      \
      echo "=== perf annotate (hottest symbol) ===" && \
      (perf annotate --stdio -i perf.data 2>/dev/null | head -n 80 || true) && \
      echo "" && \
      \
      echo "=== flamegraph ===" && \
      perf script -i perf.data | perl /opt/FlameGraph/stackcollapse-perf.pl | perl /opt/FlameGraph/flamegraph.pl > flamegraph.svg && \
      echo "Written: flamegraph.svg" && \
      echo "" && \
      \
      echo "=== cachegrind (cache + branch simulation) ===" && \
      valgrind --tool=cachegrind --cachegrind-out-file=cachegrind.out --branch-sim=yes $BIN --benchmark_min_time=0.01s 2>&1 && \
      echo "" && \
      echo "=== cg_annotate (cachegrind results) ===" && \
      cg_annotate cachegrind.out && \
      echo "Written: cachegrind.out" \
    '

# Format sources in place
fmt:
    {{run}} 'find src test bench -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format -i'

# Verify sources are formatted
fmt-check:
    {{run}} 'find src test bench -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format --dry-run --Werror'

# Run clang-tidy over src/ (configures build/ first to get compile_commands.json)
lint:
    {{run}} 'cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build && find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-tidy -p build'

# Remove build artifacts (host-side)
clean:
    rm -rf build build-bench build-profile perf.data perf.data.old flamegraph.svg cachegrind.out
