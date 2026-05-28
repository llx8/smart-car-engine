#include <gtest/gtest.h>
#include "SharedMemoryManager.hpp"

TEST(SharedMemoryManager, Singleton) {
    auto& a = SharedMemoryManager::getInstance();
    auto& b = SharedMemoryManager::getInstance();
    EXPECT_EQ(&a, &b);
}

TEST(SharedMemoryManager, InitAndShutdown) {
    auto& mgr = SharedMemoryManager::getInstance();
    mgr.init();
    mgr.shutdown();
}

TEST(SharedMemoryManager, UpdateAndGetField) {
    auto& mgr = SharedMemoryManager::getInstance();
    mgr.init();

    mgr.update("door", "front_left", "1");
    auto val = mgr.GetField("door", "front_left");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "1");

    mgr.shutdown();
}

TEST(SharedMemoryManager, GetModuleStatus) {
    auto& mgr = SharedMemoryManager::getInstance();
    mgr.init();

    mgr.update("door", "front_left", "1");
    mgr.update("door", "front_right", "0");

    auto status = mgr.GetModuleStatus("door");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status.value()["front_left"], "1");
    EXPECT_EQ(status.value()["front_right"], "0");

    mgr.shutdown();
}

TEST(SharedMemoryManager, GetAllStatus) {
    auto& mgr = SharedMemoryManager::getInstance();
    mgr.init();

    mgr.update("door", "front_left", "1");
    mgr.update("air", "ac_switch", "1");

    auto all = mgr.GetAllStatus();
    ASSERT_TRUE(all.has_value());
    EXPECT_EQ(all.value()["door"]["front_left"], 1);
    EXPECT_EQ(all.value()["air"]["ac_switch"], 1);

    mgr.shutdown();
}

TEST(SharedMemoryManager, GetFieldWithoutInit) {
    auto& mgr = SharedMemoryManager::getInstance();
    auto val = mgr.GetField("door", "front_left");
    EXPECT_FALSE(val.has_value());
}
