#include <benchmark/benchmark.h>
#include <spsc/spsc_queue.h>

#include <cstdint>
#include <optional>
#include <ranges>
#include <thread>

namespace {

void BM_SpscRoundTrip(benchmark::State& state) {
  toyspsc::SpscQueue<std::uint64_t, 1024> q;
  std::uint64_t seq = 0;
  // benchmark::State iteration is framework API, not a data loop.
  for (auto _ : state) {
    benchmark::DoNotOptimize(q.try_push(seq));
    auto val = q.try_pop();
    benchmark::DoNotOptimize(val);
    ++seq;
  }
}
BENCHMARK(BM_SpscRoundTrip);

void BM_SpscThroughput(benchmark::State& state) {
  const auto num_items = static_cast<std::uint64_t>(state.range(0));
  toyspsc::SpscQueue<std::uint64_t, 4096> q;

  for (auto _ : state) {
    auto producer = std::jthread([&q, num_items] {
      std::ranges::for_each(std::views::iota(std::uint64_t{0}, num_items), [&q](std::uint64_t i) {
        while (!q.try_push(i)) {
        }
      });
    });

    std::ranges::for_each(std::views::iota(std::uint64_t{0}, num_items), [&q](std::uint64_t) {
      std::optional<std::uint64_t> val;
      while (!(val = q.try_pop())) {
      }
      benchmark::DoNotOptimize(val);
    });

    producer.join();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(num_items));
}
BENCHMARK(BM_SpscThroughput)->Arg(1'000'000);

}  // namespace
