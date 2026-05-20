#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "AsyncAuditLogger.hpp"

TEST(AsyncAuditLogger, Singleton) {
    auto& a = AsyncAuditLogger::getInstance();
    auto& b = AsyncAuditLogger::getInstance();
    EXPECT_EQ(&a, &b);
}

TEST(AsyncAuditLogger, EnqueueNonBlocking) {
    auto& logger = AsyncAuditLogger::getInstance();

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        logger.enqueue("TEST", "test_action", 10.0f, "test");
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(ms, 100);
}

TEST(AsyncAuditLogger, QueueDropOld) {
    auto& logger = AsyncAuditLogger::getInstance();

    for (int i = 0; i < 20000; ++i) {
        logger.enqueue("TEST", "overflow_test", static_cast<float>(i), "overflow");
    }

    EXPECT_LE(logger.queueSize(), 10000);
}

TEST(AsyncAuditLogger, ShutdownWithoutInit) {
    AsyncAuditLogger::getInstance().shutdown();
}
