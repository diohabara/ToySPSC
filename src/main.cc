#include <spsc/spsc_queue.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <print>
#include <ranges>
#include <thread>

auto main() -> int {
  toyspsc::SpscQueue<std::int32_t, 8> queue;

  auto producer = std::jthread([&queue] {
    std::ranges::for_each(std::views::iota(std::int32_t{0}, std::int32_t{5}),
                          [&queue](std::int32_t i) {
                            while (!queue.try_push(i)) {
                              std::this_thread::yield();
                            }
                            std::println("Pushed: {}", i);
                          });
  });

  auto consumer = std::jthread([&queue] {
    std::ranges::for_each(std::views::iota(std::int32_t{0}, std::int32_t{5}),
                          [&queue]([[maybe_unused]] std::int32_t) {
                            std::optional<std::int32_t> val;
                            while (!(val = queue.try_pop())) {
                              std::this_thread::yield();
                            }
                            std::println("Popped: {}", *val);
                          });
  });
}
