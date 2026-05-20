#include "AsyncAuditLogger.hpp"
#include <fstream>
#include <iostream>
#include <sqlite3.h>

AsyncAuditLogger& AsyncAuditLogger::getInstance() {
    static AsyncAuditLogger instance;
    return instance;
}

AsyncAuditLogger::~AsyncAuditLogger() {
    shutdown();
}

void AsyncAuditLogger::parseConfig(const std::string& conf_path) {
    std::ifstream file(conf_path);
    if (!file.is_open()) {
        std::cerr << "[AsyncAuditLogger] Config file not found: " << conf_path
                  << ", using defaults\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(val);

        if (key == "db_path") m_db_path = val;
        else if (key == "audit_queue_max_size") m_max_queue_size = static_cast<size_t>(std::stoul(val));
    }
}

bool AsyncAuditLogger::initDB() {
    sqlite3* db = nullptr;
    if (sqlite3_open(m_db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[AsyncAuditLogger] SQLite open failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);  // 即使 open 失败也必须 close
        return false;
    }

    m_db = db;

    // WAL模式
    char* errMsg = nullptr;
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }

    const char* createTable = R"(
        CREATE TABLE IF NOT EXISTS audit_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            event_type TEXT NOT NULL,
            action TEXT NOT NULL,
            speed REAL NOT NULL,
            reason TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_timestamp ON audit_logs(timestamp);
        CREATE INDEX IF NOT EXISTS idx_event_type ON audit_logs(event_type);
    )";

    errMsg = nullptr;
    if (sqlite3_exec(db, createTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "[AsyncAuditLogger] Create table failed: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        m_db = nullptr;
        return false;
    }

    m_connected = true;
    return true;
}

bool AsyncAuditLogger::insertLog(const AuditLogEntry& entry) {
    if (!m_connected || !m_db) return false;

    auto* db = static_cast<sqlite3*>(m_db);

    const char* sql = "INSERT INTO audit_logs (event_type, action, speed, reason) VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[AsyncAuditLogger] Prepare failed: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, entry.event_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, static_cast<double>(entry.speed));
    sqlite3_bind_text(stmt, 4, entry.reason.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        std::cerr << "[AsyncAuditLogger] Insert failed: " << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);
    return success;
}

void AsyncAuditLogger::workerLoop() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });

        if (!m_running && m_queue.empty()) break;

        std::deque<AuditLogEntry> batch;
        batch.swap(m_queue);
        lock.unlock();

        for (const auto& entry : batch) {
            insertLog(entry);
        }
    }
}

void AsyncAuditLogger::init(const std::string& conf_path) {
    if (m_running) return;

    parseConfig(conf_path);
    if (!initDB()) {
        std::cerr << "[AsyncAuditLogger] DB init failed, logger will not start\n";
        return;
    }

    m_running = true;
    m_worker = std::thread(&AsyncAuditLogger::workerLoop, this);
}

void AsyncAuditLogger::enqueue(const std::string& event_type,
                                const std::string& action,
                                float speed,
                                const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    if (m_queue.size() >= m_max_queue_size) {
        m_queue.pop_front();
    }

    m_queue.push_back({event_type, action, speed, reason});
    m_cv.notify_one();
}

void AsyncAuditLogger::shutdown() {
    if (!m_running) return;

    m_running = false;
    m_cv.notify_one();

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (m_db) {
        sqlite3_close(static_cast<sqlite3*>(m_db));
        m_db = nullptr;
    }
    m_connected = false;
}

size_t AsyncAuditLogger::queueSize() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return m_queue.size();
}
