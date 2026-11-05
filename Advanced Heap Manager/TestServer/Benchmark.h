#pragma once
#include "AdvancedHeapManager.h"

struct BenchmarkResult
{
    double elapsedSeconds;     // Ukupno vreme testa
    int totalMessages;         // Ukupan broj poruka
    bool useAHM;               // true = AHM, false = malloc/free
    AHM::Strategy strategy;    // vazi samo ako je useAHM == true
};

// Pokrece benchmark test (server + vise klijenata)
BenchmarkResult RunBenchmark(bool useAHM, int clientCount, int messagesPerClient, AHM::Strategy strategy = AHM::Strategy::Map);