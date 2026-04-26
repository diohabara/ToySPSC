#ifndef TOYSPSC_SPSC_QUEUE_H_
#define TOYSPSC_SPSC_QUEUE_H_

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace toyspsc {

// std::hardware_destructive_interference_size is not ABI-stable and triggers
// -Winterference-size on GCC. Use a fixed constant instead.
inline constexpr std::size_t kCacheLineSize = 64;

template <typename T, std::size_t Capacity>
  requires std::is_trivially_copyable_v<T>
class SpscQueue {
  static_assert(Capacity > 0, "Capacity must be positive");
  static_assert(std::has_single_bit(Capacity), "Capacity must be a power of two");

  static constexpr std::size_t kMask = Capacity - 1;

 public:
  SpscQueue() = default;
  SpscQueue(const SpscQueue&) = delete;
  SpscQueue(SpscQueue&&) = delete;
  auto operator=(const SpscQueue&) -> SpscQueue& = delete;
  auto operator=(SpscQueue&&) -> SpscQueue& = delete;
  ~SpscQueue() = default;

  /// Producer-side: enqueue a value. Returns false if the queue is full.
  [[nodiscard]] auto try_push(const T& value) noexcept -> bool {
    const auto tail = tail_.load(std::memory_order_relaxed);
    const auto head = head_.load(std::memory_order_acquire);
    if (tail - head == Capacity) {
      return false;
    }
    buffer_[tail & kMask] = value;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  /// Consumer-side: dequeue a value. Returns std::nullopt if the queue is empty.
  [[nodiscard]] auto try_pop() noexcept -> std::optional<T> {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
      return std::nullopt;
    }
    auto value = buffer_[head & kMask];
    head_.store(head + 1, std::memory_order_release);
    return value;
  }

 private:
  alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
  alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
  alignas(kCacheLineSize) std::array<T, Capacity> buffer_{};
};

}  // namespace toyspsc

#endif  // TOYSPSC_SPSC_QUEUE_H_
