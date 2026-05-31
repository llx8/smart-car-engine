#pragma once
#include "CarData.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <atomic>
#include <csignal>
#include <unistd.h>
#include <string>
#include <cstring>
#include <iostream>
#include <cerrno>

static std::atomic<bool> g_keep_running{true};

inline void setupModuleSignalHandlers() {
    struct sigaction sa{};
    sa.sa_handler = [](int) { g_keep_running = false; };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

class ModuleServer {
public:
    ModuleServer(std::string path, std::string name) : m_path(std::move(path)), m_name(std::move(name)) {}

    virtual ~ModuleServer() {
        if (m_server_fd != -1) close(m_server_fd);
        if (m_epoll_fd != -1) close(m_epoll_fd);
        unlink(m_path.c_str());
        std::cout << m_name << " 资源已安全释放。" << std::endl;
    }

    void start() {
        m_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_server_fd < 0) {
            perror("socket");
            return;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_path.c_str(), sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
        unlink(m_path.c_str());
        if (bind(m_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            perror("bind");
            close(m_server_fd);
            m_server_fd = -1;
            return;
        }

        if (listen(m_server_fd, 5) < 0) {
            perror("listen");
            close(m_server_fd);
            m_server_fd = -1;
            unlink(m_path.c_str());
            return;
        }
        
        m_epoll_fd = epoll_create1(0);
        if (m_epoll_fd < 0) {
            perror("epoll_create1");
            close(m_server_fd);
            m_server_fd = -1;
            unlink(m_path.c_str());
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = m_server_fd;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_server_fd, &ev) < 0) {
            perror("epoll_ctl");
            close(m_epoll_fd);
            close(m_server_fd);
            m_epoll_fd = -1;
            m_server_fd = -1;
            unlink(m_path.c_str());
            return;
        }

        std::cout << m_name << " 服务已启动，监听路径: " << m_path << "\n";

        runLoop();
    }

protected:
    virtual void processCommand(const Car::Msg&req, Car::Msg& resp) = 0;

private:
    static bool readFull(int fd, void* buf, size_t size) {
        char* p = static_cast<char*>(buf);
        size_t total = 0;
        while (total < size) {
            ssize_t n = read(fd, p + total, size - total);
            if (n == 0) {
                return false;
            }
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            total += static_cast<size_t>(n);
        }
        return true;
    }

    static bool writeFull(int fd, const void* buf, size_t size) {
        const char* p = static_cast<const char*>(buf);
        size_t total = 0;
        while (total < size) {
            ssize_t n = send(fd, p + total, size - total, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return false;
            }
            total += static_cast<size_t>(n);
        }
        return true;
    }

    void runLoop() {
        epoll_event events[10];
        while (g_keep_running) {
            int nfds = epoll_wait(m_epoll_fd, events, 10, 1000);
            if (nfds < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("epoll_wait");
                break;
            }
            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                if (fd == m_server_fd) {
                    int client_fd = accept(m_server_fd, nullptr, nullptr);
                    if (client_fd < 0) {
                        continue;
                    }
                    epoll_event ev{EPOLLIN, {.fd = client_fd}};
                    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        close(client_fd);
                    }
                }
                else {
                    Car::Msg req{}, resp{};
                    if (!readFull(fd, &req, sizeof(req))) {
                        close(fd);
                        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        continue;
                    }
                    if (!Car::isValidMsg(req)) {
                        close(fd);
                        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        continue;
                    }

                    resp.msg_type = Car::MsgType::RESPONSE;
                    resp.mod_id = req.mod_id;
                    resp.result = 0;

                    processCommand(req, resp);

                    if (!writeFull(fd, &resp, sizeof(resp))) {
                        close(fd);
                        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    }
                }
            }
        }
    }
    std::string m_path, m_name;
    int m_server_fd{-1}, m_epoll_fd{-1};
};