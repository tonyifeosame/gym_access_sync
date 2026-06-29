#pragma once

#include <atomic>
#include <string>
#include <thread>

class RestServer {
public:
    struct Config {
        std::string db_path = "members.db";
        std::string api_key = "gym-secret-key";
        int port = 8080;
    };

    explicit RestServer(Config config);
    ~RestServer();

    RestServer(const RestServer&) = delete;
    RestServer& operator=(const RestServer&) = delete;

    bool initializeDatabaseSchema();
    bool startInBackground();
    void requestStop();
    void waitUntilStopped();

private:
    void serverLoop();

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread server_thread_;
#ifdef _WIN32
    uintptr_t server_socket_ = static_cast<uintptr_t>(-1);
#else
    int server_socket_ = -1;
#endif
};
