# ToySPSC host-side task runner.
# Every recipe runs inside the `toyspsc:dev` Linux container via Podman.
# Host requirements: `just` and `podman` only.

image_tag := "toyspsc:dev"
run := "podman run --rm -v $PWD:/work -w /work " + image_tag + " bash -lc"
run_privileged := "podman run --rm --cap-add SYS_ADMIN --security-opt seccomp=unconfined -v $PWD:/work -w /work " + image_tag + " bash -lc"

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

# Sample with perf and print a CUI report
profile:
    {{run_privileged}} 'cmake -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOYSPSC_ENABLE_BENCHMARKS=ON && cmake --build build-profile && perf stat -e cycles,instructions,cache-references,cache-misses,branch-misses ./build-profile/bench/toyspsc_bench && perf record -g --call-graph=dwarf -o perf.data ./build-profile/bench/toyspsc_bench && perf report --stdio -i perf.data | head -n 100'

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
    rm -rf build build-bench build-profile perf.data perf.data.old
