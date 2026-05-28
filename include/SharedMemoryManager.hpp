#pragma once

#include "CarData.hpp"
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <pthread.h>
#include <nlohmann/json.hpp>

constexpr uint32_t SHM_MAGIC = 0xCA123456;

struct SharedMemoryHeader {
    uint32_t magic = SHM_MAGIC;
    pthread_mutex_t mutex;
    uint32_t version = 0;
};

struct SharedMemoryData {
    Car::DoorState door;
    Car::StatusState status;
    Car::AirState air;
    Car::FaultState fault;
};

struct SharedMemoryLayout {
    SharedMemoryHeader header;
    SharedMemoryData data;
};

class SharedMemoryManager {
public:
    static SharedMemoryManager& getInstance();

    void init();
    void shutdown();

    void update(const std::string& module, const std::string& field, const std::string& value);
    void updateModule(const std::string& module, const std::unordered_map<std::string, std::string>& fields);

    std::optional<std::string> GetField(const std::string& module, const std::string& field);
    std::optional<std::unordered_map<std::string, std::string>> GetModuleStatus(const std::string& module);
    std::optional<nlohmann::json> GetAllStatus();

    SharedMemoryManager(const SharedMemoryManager&) = delete;
    SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;

private:
    SharedMemoryManager() = default;
    ~SharedMemoryManager();

    void initSharedMemory();
    void lock();
    void unlock();

    void* m_shm_ptr = nullptr;
    int m_shm_fd = -1;
    bool m_initialized = false;
};
