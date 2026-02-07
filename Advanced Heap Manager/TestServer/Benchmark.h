#pragma once

struct BenchmarkResult
{
    double elapsedSeconds;   // Ukupno vreme testa
    int totalMessages;       // Ukupan broj poruka
    bool useAHM;             // true = AHM, false = malloc/free
};

// Pokrece benchmark test (server + vise klijenata)
BenchmarkResult RunBenchmark(bool useAHM, int clientCount, int messagesPerClient);

