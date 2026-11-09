#include "BenchmarkAHM.h"
#include "AdvancedHeapManager.h"
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>

using std::vector;
using std::thread;
using std::chrono::high_resolution_clock;
using std::chrono::duration;

const size_t TOTAL_MEMORY = 4ULL * 1024 * 1024 * 1024; // 4 GB total
const size_t BLOCK_SIZE = 64 * 1024;
const int HEAP_COUNT = 8;
const size_t HEAP_SIZE_BYTES = TOTAL_MEMORY / HEAP_COUNT;

// ---------- AHM worker ----------
static void ahm_worker(size_t bytesPerThread)
{
    constexpr size_t PAGE_SIZE = 4096;
    size_t allocations = bytesPerThread / BLOCK_SIZE;
    size_t remainder = bytesPerThread % BLOCK_SIZE; // Remaining bytes that do not fit into a full block

    std::vector<void*> ptrs;
    ptrs.reserve(allocations + (remainder ? 1 : 0)); // Reserve an additional slot for the remainder

    // Allocate full-size blocks
    for (size_t i = 0; i < allocations; i++)
    {
        void* p = AHM::Malloc(BLOCK_SIZE);
        if (!p)
            break;

        // Write 0 to each memory page to prevent lazy allocation
        char* c = static_cast<char*>(p);
        for (size_t off = 0; off < BLOCK_SIZE; off += PAGE_SIZE)
            c[off] = 0;

        ptrs.push_back(p);
    }

    // Allocate the remaining bytes, if any
    if (remainder > 0)
    {
        void* p = AHM::Malloc(remainder);
        if (p)
        {
            char* c = static_cast<char*>(p);
            for (size_t off = 0; off < remainder; off += PAGE_SIZE)
                c[off] = 0;

            ptrs.push_back(p);
        }

    }

    // Deallocate memory
    for (void* p : ptrs)
        AHM::Free(p);
}


// ---------- AHM test for N threads ----------
static BenchmarkResult run_ahm_test_for_threads(int threadCount, AHM::Strategy strategy)
{
    size_t bytesPerThread = TOTAL_MEMORY / threadCount;

    // Initialize AHM
    AHM::Initialize(HEAP_COUNT, HEAP_SIZE_BYTES, strategy);

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

    // Shut down AHM
    AHM::Shutdown();

    return { threadCount, timeSeconds };
}

// ---------- Public function ----------
void run_ahm_benchmark(const vector<int>& threadCounts, vector<BenchmarkResult>& results, AHM::Strategy strategy)
{
    std::cout << "\n========================================\n";
    std::cout << "Benchmark - "
        << (strategy == AHM::Strategy::Header ? "AHM (Header)" : "AHM (Map)")
        << "\n";
    std::cout << "========================================\n";

    for (int tc : threadCounts)
    {
        std::cout << "Threads: " << tc << " ... ";
        std::cout.flush();

        BenchmarkResult result = run_ahm_test_for_threads(tc, strategy);
        results.push_back(result);

        std::cout << result.timeSeconds << " s\n";
    }
}