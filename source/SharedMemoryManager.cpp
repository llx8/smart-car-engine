#include "SharedMemoryManager.hpp"
#include "CarData.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <iostream>

SharedMemoryManager& SharedMemoryManager::getInstance() {
    static SharedMemoryManager instance;
    return instance;
}

SharedMemoryManager::~SharedMemoryManager() {
    shutdown();
}

void SharedMemoryManager::initSharedMemory() {
    m_shm_fd = shm_open("/car_status", O_CREAT | O_RDWR, 0666);
    if (m_shm_fd < 0) {
        std::cerr << "[SharedMemoryManager] shm_open failed\n";
        return;
    }

    if (ftruncate(m_shm_fd, sizeof(SharedMemoryLayout)) < 0) {
        std::cerr << "[SharedMemoryManager] ftruncate failed\n";
        close(m_shm_fd);
        m_shm_fd = -1;
        return;
    }

    m_shm_ptr = mmap(nullptr, sizeof(SharedMemoryLayout), PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
    if (m_shm_ptr == MAP_FAILED) {
        std::cerr << "[SharedMemoryManager] mmap failed\n";
        close(m_shm_fd);
        m_shm_fd = -1;
        m_shm_ptr = nullptr;
        return;
    }

    auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);
    if (layout->header.magic != SHM_MAGIC) {
        layout->header.magic = SHM_MAGIC;
        layout->header.version = 0;
        std::memset(&layout->data, 0, sizeof(SharedMemoryData));

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&layout->header.mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    m_initialized = true;
}

void SharedMemoryManager::init() {
    if (m_initialized) return;
    initSharedMemory();
}

void SharedMemoryManager::shutdown() {
    if (!m_initialized) return;

    if (m_shm_ptr && m_shm_ptr != MAP_FAILED) {
        munmap(m_shm_ptr, sizeof(SharedMemoryLayout));
    }
    if (m_shm_fd >= 0) {
        close(m_shm_fd);
    }

    m_shm_ptr = nullptr;
    m_shm_fd = -1;
    m_initialized = false;
}

void SharedMemoryManager::lock() {
    if (m_initialized) {
        auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);
        pthread_mutex_lock(&layout->header.mutex);
    }
}

void SharedMemoryManager::unlock() {
    if (m_initialized) {
        auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);
        pthread_mutex_unlock(&layout->header.mutex);
    }
}

void SharedMemoryManager::update(const std::string& module, const std::string& field, const std::string& value) {
    if (!m_initialized) return;

    lock();
    try {
        auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);

        if (module == "door") {
            auto& door = layout->data.door;
            if (field == "front_left") door.front_left = static_cast<uint8_t>(std::stoi(value));
            else if (field == "front_right") door.front_right = static_cast<uint8_t>(std::stoi(value));
            else if (field == "back_left") door.back_left = static_cast<uint8_t>(std::stoi(value));
            else if (field == "back_right") door.back_right = static_cast<uint8_t>(std::stoi(value));
            else if (field == "trunk") door.trunk = static_cast<uint8_t>(std::stoi(value));
            else if (field == "lock_status") door.lock_status = static_cast<uint8_t>(std::stoi(value));
        } else if (module == "status") {
            auto& status = layout->data.status;
            if (field == "speed") status.speed = std::stof(value);
            else if (field == "rpm") status.rpm = std::stoi(value);
            else if (field == "water_temp") status.water_temp = std::stof(value);
            else if (field == "oil_temp") status.oil_temp = std::stof(value);
            else if (field == "fuel") status.fuel = std::stof(value);
            else if (field == "battery_voltage") status.battery_voltage = std::stof(value);
            else if (field == "gear") status.gear = static_cast<uint8_t>(std::stoi(value));
            else if (field == "hand_brake") status.hand_brake = static_cast<uint8_t>(std::stoi(value));
        } else if (module == "air") {
            auto& air = layout->data.air;
            if (field == "ac_switch") air.ac_switch = static_cast<uint8_t>(std::stoi(value));
            else if (field == "fan_speed") air.fan_speed = static_cast<uint8_t>(std::stoi(value));
            else if (field == "temp_set") air.temp_set = std::stoi(value);
            else if (field == "inner_cycle") air.inner_cycle = static_cast<uint8_t>(std::stoi(value));
        } else if (module == "fault") {
            auto& fault = layout->data.fault;
            if (field == "fault_count") fault.fault_count = static_cast<uint8_t>(std::stoi(value));
            else if (field == "wring_light") fault.wring_light = static_cast<uint8_t>(std::stoi(value));
        }

        layout->header.version++;
    } catch (...) {
        unlock();
        throw;
    }
    unlock();
}

void SharedMemoryManager::updateModule(const std::string& module, const std::unordered_map<std::string, std::string>& fields) {
    for (const auto& [field, value] : fields) {
        update(module, field, value);
    }
}

std::optional<std::string> SharedMemoryManager::GetField(const std::string& module, const std::string& field) {
    if (!m_initialized) return std::nullopt;

    lock();
    auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);
    std::string result;

    if (module == "door") {
        const auto& door = layout->data.door;
        if (field == "front_left") result = std::to_string(door.front_left);
        else if (field == "front_right") result = std::to_string(door.front_right);
        else if (field == "back_left") result = std::to_string(door.back_left);
        else if (field == "back_right") result = std::to_string(door.back_right);
        else if (field == "trunk") result = std::to_string(door.trunk);
        else if (field == "lock_status") result = std::to_string(door.lock_status);
    } else if (module == "status") {
        const auto& status = layout->data.status;
        if (field == "speed") result = std::to_string(status.speed);
        else if (field == "rpm") result = std::to_string(status.rpm);
        else if (field == "water_temp") result = std::to_string(status.water_temp);
        else if (field == "oil_temp") result = std::to_string(status.oil_temp);
        else if (field == "fuel") result = std::to_string(status.fuel);
        else if (field == "battery_voltage") result = std::to_string(status.battery_voltage);
        else if (field == "gear") result = std::to_string(status.gear);
        else if (field == "hand_brake") result = std::to_string(status.hand_brake);
    } else if (module == "air") {
        const auto& air = layout->data.air;
        if (field == "ac_switch") result = std::to_string(air.ac_switch);
        else if (field == "fan_speed") result = std::to_string(air.fan_speed);
        else if (field == "temp_set") result = std::to_string(air.temp_set);
        else if (field == "inner_cycle") result = std::to_string(air.inner_cycle);
    } else if (module == "fault") {
        const auto& fault = layout->data.fault;
        if (field == "fault_count") result = std::to_string(fault.fault_count);
        else if (field == "wring_light") result = std::to_string(fault.wring_light);
    }

    unlock();

    if (result.empty()) return std::nullopt;
    return result;
}

std::optional<std::unordered_map<std::string, std::string>> SharedMemoryManager::GetModuleStatus(const std::string& module) {
    if (!m_initialized) return std::nullopt;

    std::unordered_map<std::string, std::string> result;

    lock();
    auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);

    if (module == "door") {
        const auto& door = layout->data.door;
        result["front_left"] = std::to_string(door.front_left);
        result["front_right"] = std::to_string(door.front_right);
        result["back_left"] = std::to_string(door.back_left);
        result["back_right"] = std::to_string(door.back_right);
        result["trunk"] = std::to_string(door.trunk);
        result["lock_status"] = std::to_string(door.lock_status);
    } else if (module == "status") {
        const auto& status = layout->data.status;
        result["speed"] = std::to_string(status.speed);
        result["rpm"] = std::to_string(status.rpm);
        result["water_temp"] = std::to_string(status.water_temp);
        result["oil_temp"] = std::to_string(status.oil_temp);
        result["fuel"] = std::to_string(status.fuel);
        result["battery_voltage"] = std::to_string(status.battery_voltage);
        result["gear"] = std::to_string(status.gear);
        result["hand_brake"] = std::to_string(status.hand_brake);
    } else if (module == "air") {
        const auto& air = layout->data.air;
        result["ac_switch"] = std::to_string(air.ac_switch);
        result["fan_speed"] = std::to_string(air.fan_speed);
        result["temp_set"] = std::to_string(air.temp_set);
        result["inner_cycle"] = std::to_string(air.inner_cycle);
    } else if (module == "fault") {
        const auto& fault = layout->data.fault;
        result["fault_count"] = std::to_string(fault.fault_count);
        result["wring_light"] = std::to_string(fault.wring_light);
    }

    unlock();

    if (result.empty()) return std::nullopt;
    return result;
}

std::optional<nlohmann::json> SharedMemoryManager::GetAllStatus() {
    if (!m_initialized) return std::nullopt;

    nlohmann::json j;

    lock();
    auto* layout = static_cast<SharedMemoryLayout*>(m_shm_ptr);

    j["door"] = {
        {"front_left", layout->data.door.front_left},
        {"front_right", layout->data.door.front_right},
        {"back_left", layout->data.door.back_left},
        {"back_right", layout->data.door.back_right},
        {"trunk", layout->data.door.trunk},
        {"lock_status", layout->data.door.lock_status}
    };

    j["status"] = {
        {"speed", layout->data.status.speed},
        {"rpm", layout->data.status.rpm},
        {"water_temp", layout->data.status.water_temp},
        {"oil_temp", layout->data.status.oil_temp},
        {"fuel", layout->data.status.fuel},
        {"battery_voltage", layout->data.status.battery_voltage},
        {"gear", layout->data.status.gear},
        {"hand_brake", layout->data.status.hand_brake}
    };

    j["air"] = {
        {"ac_switch", layout->data.air.ac_switch},
        {"fan_speed", layout->data.air.fan_speed},
        {"temp_set", layout->data.air.temp_set},
        {"inner_cycle", layout->data.air.inner_cycle}
    };

    j["fault"] = {
        {"fault_count", layout->data.fault.fault_count},
        {"wring_light", layout->data.fault.wring_light}
    };

    unlock();

    return j;
}
