#include "api_client.h"
#include "database.h"
#include "rest_server.h"
#include "sync.h"

#include <atomic>
#include <chrono>
#include <csignal>
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

    logInfo("Gym access terminal starting");
    logInfo("API URL: " + config.api_url);
    logInfo("Sync interval: " + std::to_string(config.sync_interval_seconds) + " seconds");
    logInfo("REST server port: " + std::to_string(config.server_port));

    RestServer::Config serverConfig;
    serverConfig.db_path = "members.db";
    serverConfig.api_key = config.api_key;
    serverConfig.port = config.server_port;

    RestServer server(serverConfig);
    if (!server.initializeDatabaseSchema()) {
        logError("Failed to initialize database schema");
        return 1;
    }

    if (!server.startInBackground()) {
        logError("Failed to start local REST server");
        return 1;
    }

    sqlite3* db = nullptr;
    if (!openDatabase(db, "members.db")) {
        logError("Failed to open database for sync");
        server.requestStop();
        server.waitUntilStopped();
        return 1;
    }

    ApiClient apiClient(config);
    runSyncCycle(db, apiClient);

    const auto syncInterval = std::chrono::seconds(config.sync_interval_seconds);
    logInfo("Service running. Press Ctrl+C to stop.");

    while (!g_shutdown.load()) {
        const auto wakeAt = std::chrono::steady_clock::now() + syncInterval;
        while (!g_shutdown.load() && std::chrono::steady_clock::now() < wakeAt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (!g_shutdown.load()) {
            runSyncCycle(db, apiClient);
        }
    }

    logInfo("Shutdown requested. Stopping services...");
    server.requestStop();
    server.waitUntilStopped();
    closeDatabase(db);
    logInfo("Gym access terminal stopped");
    return 0;
}
