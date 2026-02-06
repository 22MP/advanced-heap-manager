#include "Benchmark.h"
#include "AdvancedHeapManager.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <string>

using std::cout;
using std::vector;
using std::thread;
using std::chrono::high_resolution_clock;
using std::chrono::duration;

const size_t TOTAL_MEMORY = 512ULL * 1024 * 1024; 
const size_t BLOCK_SIZE = 2048; // 1KB
const int HEAP_COUNT = 8;

// Struktura za rezultate
struct BenchmarkResult
{
    int threadCount;
    double timeSeconds;
};

// ---------- malloc/free worker ----------
static void malloc_worker(size_t bytesPerThread)
{
    size_t allocations = bytesPerThread / BLOCK_SIZE;
    vector<void*> ptrs;
    ptrs.reserve(allocations);

    for (size_t i = 0; i < allocations; i++)
    {
        void* p = malloc(BLOCK_SIZE);
        if (p)
            ptrs.push_back(p);
    }

    for (void* p : ptrs)
        free(p);
}

// ---------- AHM worker ----------
static void ahm_worker(size_t bytesPerThread)
{
    size_t allocations = bytesPerThread / BLOCK_SIZE;
    vector<void*> ptrs;
    ptrs.reserve(allocations);

    for (size_t i = 0; i < allocations; i++)
    {
        void* p = AHM::Malloc(BLOCK_SIZE);
        if (p)
            ptrs.push_back(p);
    }

    for (void* p : ptrs)
        AHM::Free(p);
}

// ---------- malloc test for N threads ----------
static BenchmarkResult run_malloc_test_for_threads(int threadCount)
{
    size_t bytesPerThread = TOTAL_MEMORY / threadCount;

    auto start = high_resolution_clock::now();
    {
        vector<thread> threads;
        for (int i = 0; i < threadCount; i++)
            threads.emplace_back(malloc_worker, bytesPerThread);

        for (auto& t : threads)
            t.join();
    }
    auto end = high_resolution_clock::now();

    double timeSeconds = duration<double>(end - start).count();
    double throughputMBps = (TOTAL_MEMORY / (1024.0 * 1024.0)) / timeSeconds;

    return { threadCount, timeSeconds};
}

// ---------- AHM test for N threads ----------
static BenchmarkResult run_ahm_test_for_threads(int threadCount)
{
    size_t bytesPerThread = TOTAL_MEMORY / threadCount;

    // Initialize AHM with HEAP_COUNT heaps
    size_t heapSizeBytes = 256 * 1024 * 1024; // 256 MB initial size per heap
    AHM::Initialize(HEAP_COUNT, heapSizeBytes);

    auto start = high_resolution_clock::now();
    {
        vector<thread> threads;
        for (int i = 0; i < threadCount; i++)
            threads.emplace_back(ahm_worker, bytesPerThread);

        for (auto& t : threads)
            t.join();
    }
    auto end = high_resolution_clock::now();

    double timeSeconds = duration<double>(end - start).count();

    // Shutdown AHM after test
    AHM::Shutdown();

    return { threadCount, timeSeconds};
}

// ---------- public functions ----------

void run_malloc_benchmark()
{
    vector<int> threadCounts = { 1, 2, 5, 10, 50 };

    cout << "\n========================================\n";
    cout << "Benchmark Test - malloc/free only\n";
    cout << "========================================\n";
    cout << "Total memory: 4GB\n";
    cout << "Block size: 1KB\n\n";

    for (int tc : threadCounts)
    {
        cout << "Testing with " << tc << " thread(s)...\n";
        BenchmarkResult result = run_malloc_test_for_threads(tc);

        cout << "  Threads: " << std::setw(3) << result.threadCount
            << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << result.timeSeconds << " s";
        cout << "\n";
    }
}

void run_ahm_benchmark()
{
    vector<int> threadCounts = { 1, 2, 5, 10, 50 };

    cout << "\n========================================\n";
    cout << "Benchmark Test - Advanced Heap Manager\n";
    cout << "========================================\n";
    cout << "Total memory: 4GB\n";
    cout << "Block size: 1KB\n";
    cout << "Heap count: " << HEAP_COUNT << "\n\n";

    for (int tc : threadCounts)
    {
        cout << "Testing with " << tc << " thread(s)...\n";
        BenchmarkResult result = run_ahm_test_for_threads(tc);

        cout << "  Threads: " << std::setw(3) << result.threadCount
            << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << result.timeSeconds << " s";
        cout << "\n";
    }
}

void run_comparison_benchmark()
{
    vector<int> threadCounts = { 1, 2, 5, 10, 50 };

    cout << "\n========================================\n";
    cout << "Comparison Benchmark - malloc vs AHM\n";
    cout << "========================================\n";
    cout << "Total memory: 4GB\n";
    cout << "Block size: 1KB\n";
    cout << "Heap count: " << HEAP_COUNT << "\n\n";

    vector<BenchmarkResult> mallocResults;
    vector<BenchmarkResult> ahmResults;

    for (int tc : threadCounts)
    {
        cout << "Testing with " << tc << " thread(s)...\n";

        // malloc test
        cout << "  malloc/free: ";
        cout.flush();
        BenchmarkResult mallocResult = run_malloc_test_for_threads(tc);
        mallocResults.push_back(mallocResult);
        cout << std::fixed << std::setprecision(3) << mallocResult.timeSeconds << " s\n";

        // AHM test
        cout << "  AHM:         ";
        cout.flush();
        BenchmarkResult ahmResult = run_ahm_test_for_threads(tc);
        ahmResults.push_back(ahmResult);
        cout << std::fixed << std::setprecision(3) << ahmResult.timeSeconds << " s\n";

        // Speedup
        double speedup = mallocResult.timeSeconds / ahmResult.timeSeconds;
        cout << "  Speedup:     " << std::setprecision(2) << speedup << "x ";
        if (speedup > 1.0)
            cout << "(AHM brzi)\n";
        else
            cout << "(malloc brzi)\n";

        cout << "\n";
    }

    // Detaljna tabela
    cout << "========================================\n";
    cout << "Detailed Comparison:\n";
    cout << "========================================\n";
    cout << std::left << std::setw(10) << "Threads"
        << std::setw(12) << "malloc (s)"
        << std::setw(12) << "AHM (s)"
        << std::setw(12) << "Speedup"
        << std::setw(15) << "Improvement"
        << "\n";
    cout << std::string(65, '-') << "\n";

    for (size_t i = 0; i < threadCounts.size(); i++)
    {
        double speedup = mallocResults[i].timeSeconds / ahmResults[i].timeSeconds;
        double improvement = ((mallocResults[i].timeSeconds - ahmResults[i].timeSeconds)
            / mallocResults[i].timeSeconds) * 100.0;

        cout << std::left << std::setw(10) << threadCounts[i]
            << std::fixed << std::setprecision(3)
            << std::setw(12) << mallocResults[i].timeSeconds
            << std::setw(12) << ahmResults[i].timeSeconds
            << std::setprecision(2)
            << std::setw(12) << speedup
            << std::setprecision(1)
            << std::setw(15) << (improvement >= 0 ? "+" : "") << improvement << "%"
            << "\n";
    }

    cout << "\n";

    // Proseèni speedup
    double avgSpeedup = 0.0;
    for (size_t i = 0; i < mallocResults.size(); i++)
    {
        avgSpeedup += mallocResults[i].timeSeconds / ahmResults[i].timeSeconds;
    }
    avgSpeedup /= mallocResults.size();

    cout << "Average Speedup: " << std::fixed << std::setprecision(2)
        << avgSpeedup << "x\n\n";
}