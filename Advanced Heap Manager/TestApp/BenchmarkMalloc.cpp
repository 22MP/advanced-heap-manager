#include "BenchmarkMalloc.h"
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>

using std::vector;
using std::thread;
using std::chrono::high_resolution_clock;
using std::chrono::duration;

const size_t TOTAL_MEMORY = 4ULL * 1024 * 1024 * 1024; // 4 GB total
const size_t BLOCK_SIZE = 64 * 1024;

// ---------- malloc worker ----------
static void malloc_worker(size_t bytesPerThread)
{
    constexpr size_t PAGE_SIZE = 4096;
    size_t allocations = bytesPerThread / BLOCK_SIZE;
    size_t remainder = bytesPerThread % BLOCK_SIZE;

    std::vector<void*> ptrs;
    ptrs.reserve(allocations + (remainder ? 1 : 0));

    for (size_t i = 0; i < allocations; i++)
    {
        void* p = malloc(BLOCK_SIZE);
        if (!p)
            break;
        char* c = static_cast<char*>(p);
        for (size_t off = 0; off < BLOCK_SIZE; off += PAGE_SIZE)
            c[off] = 0;

        ptrs.push_back(p);
    }

    if (remainder > 0)
    {
        void* p = malloc(remainder);
        if (p)
        {
            char* c = static_cast<char*>(p);
            for (size_t off = 0; off < remainder; off += PAGE_SIZE)
                c[off] = 0;

            ptrs.push_back(p);
        }
    }

    // Deallocate all allocated memory blocks
    for (void* p : ptrs)
        free(p);
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

    return { threadCount, timeSeconds };
}

// ---------- Public function ----------
void run_malloc_benchmark(const vector<int>& threadCounts, vector<BenchmarkResult>& results)
{
    std::cout << "\n========================================\n";
    std::cout << "Benchmark - malloc/free\n";
    std::cout << "========================================\n";

    for (int tc : threadCounts)
    {
        std::cout << "Threads: " << tc << " ... ";
        std::cout.flush();

        BenchmarkResult result = run_malloc_test_for_threads(tc);
        results.push_back(result);

        std::cout << result.timeSeconds << " s\n";
    }
}