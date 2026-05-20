#pragma once
#include <string>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>

struct AuditLogEntry {
    std::string event_type;
    std::string action;
    float speed;
    std::string reason;
};

class AsyncAuditLogger {
public:
    static AsyncAuditLogger& getInstance();

    void init(const std::string& conf_path);
    void enqueue(const std::string& event_type,
                 const std::string& action,
                 float speed,
                 const std::string& reason);
    void shutdown();
    size_t queueSize() const;

    AsyncAuditLogger(const AsyncAuditLogger&) = delete;
    AsyncAuditLogger& operator=(const AsyncAuditLogger&) = delete;

private:
    AsyncAuditLogger() = default;
    ~AsyncAuditLogger();

    void parseConfig(const std::string& conf_path);
    void workerLoop();

    bool initDB();
    bool insertLog(const AuditLogEntry& entry);

    std::string m_db_path = "./car_audit.db";
    size_t m_max_queue_size = 10000;

    mutable std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    std::deque<AuditLogEntry> m_queue;

    std::thread m_worker;
    std::atomic<bool> m_running{false};

    void* m_db = nullptr;
    std::atomic<bool> m_connected{false};
};
#define AUDIT_LOG(type, action, speed, reason) \
    AsyncAuditLogger::getInstance().enqueue(type, action, speed, reason)
