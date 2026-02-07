#include "Server.h"
#include "Client.h"
#include "AdvancedHeapManager.h"
#include "Benchmark.h"
#include <thread>
#include <vector>
#include <chrono>

BenchmarkResult RunBenchmark(bool useAHM, int clientCount, int messagesPerClient)
{
    BenchmarkResult result{};
    result.useAHM = useAHM;
    result.totalMessages = clientCount * messagesPerClient;

    if (useAHM)
        AHM::Initialize(8, size_t(256) * 1024 * 1024);

    g_serverRunning = true;
    g_totalMessages = 0;

    std::thread serverThread(RunServer, useAHM);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> clients;
    for (int i = 0; i < clientCount; ++i)
        clients.emplace_back(RunClient, i + 1, messagesPerClient);

    for (auto& t : clients) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    result.elapsedSeconds = std::chrono::duration<double>(end - start).count();

    g_serverRunning = false;
    closesocket(g_listenSocket);
    serverThread.join();

    if (useAHM) AHM::Shutdown();

    return result;
}
