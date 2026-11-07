#include "BoundedQueue.hpp"

#include <gtest/gtest.h>

#include <string>
#include <memory>

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

TEST(BoundedQueueTest, PopOnEmptyQueueReturnsNothing) {
    BoundedQueue<std::string> q{3};
    EXPECT_FALSE(q.pop().has_value());
}

TEST(BoundedQueueTest, PushFailsWhenFull) {
    BoundedQueue<std::string> q{2};
    EXPECT_TRUE(q.push("a"));
    EXPECT_TRUE(q.push("b"));
    EXPECT_FALSE(q.push("c"));
    EXPECT_EQ(q.size(), 2u);
}

TEST(BoundedQueueTest, PushSucceedsAgainAfterPopMakesRoom) {
    BoundedQueue<std::string> q{1};
    ASSERT_TRUE(q.push("a"));
    ASSERT_FALSE(q.push("b"));

    EXPECT_EQ(q.pop(), "a");
    EXPECT_TRUE(q.push("b"));
}

TEST(BoundedQueueTest, PushFailsAfterClose) {
    BoundedQueue<std::string> q{3};
    q.close();
    EXPECT_TRUE(q.closed());
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