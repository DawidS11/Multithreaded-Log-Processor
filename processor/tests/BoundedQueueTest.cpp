#include "BoundedQueue.hpp"

#include <gtest/gtest.h>

#include <string>
#include <memory>
#include <atomic>
#include <thread>

using namespace std::chrono_literals;

using logproc::BoundedQueue;

TEST(BoundedQueueTest, PopsItemsInFirstInFirstOutOrder) {
    BoundedQueue<std::string> q{3};
    ASSERT_TRUE(q.push("a"));
    ASSERT_TRUE(q.push("b"));
    ASSERT_TRUE(q.push("c"));

    EXPECT_EQ(q.pop(), "a");
    EXPECT_EQ(q.pop(), "b");
    EXPECT_EQ(q.pop(), "c");
}


TEST(BoundedQueueTest, PopOnEmptyClosedQueueReturnsNothing) {
    BoundedQueue<std::string> q{3};
    q.close();
    EXPECT_FALSE(q.pop().has_value());
}

TEST(BoundedQueueTest, PushBlocksWhenFullUntilPopMakesRoom) {
    BoundedQueue<std::string> q{1};
    ASSERT_TRUE(q.push("a"));

    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        EXPECT_TRUE(q.push("b"));
        pushed = true;
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(pushed);  // still blocked, because the queue is full

    EXPECT_EQ(q.pop(), "a");  // makes room, which wakes the producer
    producer.join();

    EXPECT_TRUE(pushed);
    EXPECT_EQ(q.pop(), "b");
}

TEST(BoundedQueueTest, CloseWakesBlockedPop) {
    BoundedQueue<std::string> q{1};
    std::optional<std::string> result = std::string{"unset"};

    std::thread consumer([&] { result = q.pop(); });

    std::this_thread::sleep_for(50ms);
    q.close();  // without notify_all in close(), this join never returns
    consumer.join();

    EXPECT_FALSE(result.has_value());
}

TEST(BoundedQueueTest, PushFailsAfterClose) {
    BoundedQueue<std::string> q{3};
    q.close();
    EXPECT_TRUE(q.isClosed());
    EXPECT_FALSE(q.push("a"));
    EXPECT_EQ(q.size(), 0u);
}

TEST(BoundedQueueTest, PendingItemsStillPopAfterClose) {
    BoundedQueue<std::string> q{3};
    ASSERT_TRUE(q.push("a"));
    ASSERT_TRUE(q.push("b"));
    q.close();

    EXPECT_EQ(q.pop(), "a");
    EXPECT_EQ(q.pop(), "b");
    EXPECT_FALSE(q.pop().has_value());
}

TEST(BoundedQueueTest, MoveOnlyItemsAreAccepted) {
    BoundedQueue<std::unique_ptr<int>> q{2};
    EXPECT_TRUE(q.push(std::make_unique<int>(7)));

    auto item = q.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(**item, 7);
}