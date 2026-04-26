#include <gtest/gtest.h>
#include <spsc/spsc_queue.h>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <thread>
#include <vector>

namespace {

using Queue4 = toyspsc::SpscQueue<std::int32_t, 4>;

TEST(SpscQueue, PushPopSingle) {
  Queue4 q;
  ASSERT_TRUE(q.try_push(42));
  auto val = q.try_pop();
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 42);
}

TEST(SpscQueue, EmptyPopReturnsNullopt) {
  Queue4 q;
  EXPECT_EQ(q.try_pop(), std::nullopt);
}

TEST(SpscQueue, FullQueueRejectsPush) {
  Queue4 q;
  std::ranges::for_each(std::views::iota(std::int32_t{0}, std::int32_t{4}),
                        [&q](std::int32_t i) { ASSERT_TRUE(q.try_push(i)); });
  EXPECT_FALSE(q.try_push(99));
}

TEST(SpscQueue, WrapAround) {
  Queue4 q;
  // Fill and drain once to advance head/tail past Capacity.
  std::ranges::for_each(std::views::iota(std::int32_t{0}, std::int32_t{4}),
                        [&q](std::int32_t i) { ASSERT_TRUE(q.try_push(i)); });
  std::ranges::for_each(std::views::iota(std::int32_t{0}, std::int32_t{4}),
                        [&q](std::int32_t) { ASSERT_TRUE(q.try_pop().has_value()); });
  // Now push/pop again — indices wrap via bitmask.
  std::ranges::for_each(std::views::iota(std::int32_t{10}, std::int32_t{14}),
                        [&q](std::int32_t i) { ASSERT_TRUE(q.try_push(i)); });
  std::ranges::for_each(std::views::iota(std::int32_t{10}, std::int32_t{14}), [&q](std::int32_t i) {
    auto val = q.try_pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, i);
  });
}

TEST(SpscQueue, FifoOrder) {
  toyspsc::SpscQueue<std::int32_t, 16> q;
  constexpr std::int32_t kCount = 16;
  std::ranges::for_each(std::views::iota(std::int32_t{0}, kCount),
                        [&q](std::int32_t i) { ASSERT_TRUE(q.try_push(i)); });
  std::ranges::for_each(std::views::iota(std::int32_t{0}, kCount), [&q](std::int32_t i) {
    auto val = q.try_pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, i);
  });
}

TEST(SpscQueue, ProducerConsumer) {
  constexpr std::uint64_t kNumItems = 100'000;
  toyspsc::SpscQueue<std::uint64_t, 1024> q;

  auto producer = std::jthread([&q, kNumItems] {
    std::ranges::for_each(std::views::iota(std::uint64_t{0}, kNumItems), [&q](std::uint64_t i) {
      while (!q.try_push(i)) {
      }
    });
  });

  std::vector<std::uint64_t> received;
  received.reserve(kNumItems);

  auto consumer = std::jthread([&q, &received, kNumItems] {
    std::ranges::for_each(std::views::iota(std::uint64_t{0}, kNumItems),
                          [&q, &received](std::uint64_t) {
                            std::optional<std::uint64_t> val;
                            while (!(val = q.try_pop())) {
                            }
                            received.push_back(*val);
                          });
  });

  producer.join();
  consumer.join();

  ASSERT_EQ(received.size(), kNumItems);
  EXPECT_TRUE(std::ranges::equal(received, std::views::iota(std::uint64_t{0}, kNumItems)));
}

}  // namespace
