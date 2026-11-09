#pragma once
#include "AdvancedHeapManager.h"

struct BenchmarkResult
{
    double elapsedSeconds;     // Total test execution time
    int totalMessages;         // Total number of messages
    bool useAHM;               // true = AHM, false = malloc/free
    AHM::Strategy strategy;    // Applies only if useAHM == true
};

// Runs the benchmark test (server + multiple clients)
BenchmarkResult RunBenchmark(bool useAHM, int clientCount, int messagesPerClient, AHM::Strategy strategy = AHM::Strategy::Map);