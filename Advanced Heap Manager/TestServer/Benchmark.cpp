#include "Server.h"
#include "Client.h"
#include "AdvancedHeapManager.h"
#include "Benchmark.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

BenchmarkResult RunBenchmark(bool useAHM, int clientCount, int messagesPerClient,
    AHM::Strategy strategy)
{
    BenchmarkResult result{};
    result.useAHM = useAHM;
    result.strategy = strategy;
    result.totalMessages = clientCount * messagesPerClient;

    if (useAHM)
        AHM::Initialize(8, size_t(256) * 1024 * 1024, strategy);

    g_serverRunning = true;
    g_totalMessages = 0;
    g_clientsConnected = 0;
    g_startFlag = false;

    std::thread serverThread(RunServer, useAHM);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::vector<std::thread> clients;
    for (int i = 0; i < clientCount; ++i)
        clients.emplace_back(RunClient, i + 1, messagesPerClient);

    // Sacekaj da se svi klijenti povezu (najvise 10 s), pa ih pusti istovremeno
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (g_clientsConnected < clientCount &&
        std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (g_clientsConnected < clientCount)
        std::cout << "UPOZORENJE: povezalo se samo " << g_clientsConnected
        << " od " << clientCount << " klijenata\n";

    // Merenje pocinje tek kad su svi povezani
    auto start = std::chrono::high_resolution_clock::now();
    g_startFlag = true;

    for (auto& t : clients) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    result.elapsedSeconds = std::chrono::duration<double>(end - start).count();

    g_serverRunning = false;
    closesocket(g_listenSocket);
    serverThread.join();

    // Provjera: ako je obradjeno mnogo manje poruka nego sto treba, mjerenje nije pouzdano
    if (g_totalMessages < static_cast<long long>(result.totalMessages * 0.9))
        std::cout << "UPOZORENJE: obradjeno samo " << g_totalMessages
        << " od " << result.totalMessages
        << " poruka - ovo mjerenje nije pouzdano\n";

    if (useAHM) AHM::Shutdown();

    return result;
}