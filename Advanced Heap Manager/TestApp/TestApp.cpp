#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

using std::cout;
using std::vector;
using std::thread;
using std::chrono::high_resolution_clock;
using std::chrono::duration;

const size_t TOTAL_MEMORY = 512ULL * 1024 * 1024; // 512MB za prvi commit
const size_t BLOCK_SIZE = 1024; // 1KB po alokaciji

void test_malloc_free(size_t bytesPerThread)
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

void run_test(int threadCount)
{
    size_t bytesPerThread = TOTAL_MEMORY / threadCount;

    cout << "=============================\n";
    cout << "Threads: " << threadCount << "\n";

    auto start = high_resolution_clock::now();
    {
        vector<thread> threads;
        for (int i = 0; i < threadCount; i++)
            threads.emplace_back(test_malloc_free, bytesPerThread);

        for (auto& t : threads)
            t.join();
    }
    auto end = high_resolution_clock::now();

    double mallocTime = duration<double>(end - start).count();
    cout << "malloc/free time: " << mallocTime << " sec\n\n";
}

int main()
{
    vector<int> threadCounts = { 1, 2, 5, 10, 50 };

    cout << "Benchmark Test - malloc/free only\n";
    cout << "Total memory: 512MB\n";
    cout << "Block size: 1KB\n\n";

    for (int tc : threadCounts)
    {
        run_test(tc);
    }

    return 0;
}
