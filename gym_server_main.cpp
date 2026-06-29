#include "api_client.h"
#include "rest_server.h"

#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>

static std::atomic<bool> g_shutdown{false};

static void onSignal(int)
{
    g_shutdown.store(true);
}

int main()
{
    std::signal(SIGINT, onSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, onSignal);
#endif

    ApiConfig config;
    if (!loadConfig("config.json", config)) {
        std::cerr << "Failed to load config.json" << std::endl;
        return 1;
    }

    RestServer::Config serverConfig;
    serverConfig.db_path = "members.db";
    serverConfig.api_key = config.api_key;
    serverConfig.port = config.server_port;

    RestServer server(serverConfig);
    if (!server.initializeDatabaseSchema()) {
        std::cerr << "Failed to initialize database schema" << std::endl;
        return 1;
    }

    if (!server.startInBackground()) {
        std::cerr << "Failed to start REST server" << std::endl;
        return 1;
    }

    std::cout << "Gym REST server started. Press Ctrl+C to stop." << std::endl;
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "Shutting down..." << std::endl;
    server.requestStop();
    server.waitUntilStopped();
    return 0;
}
