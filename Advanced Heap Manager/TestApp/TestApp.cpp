#include <iostream>
#include <vector>
#include <iomanip>
#include "BenchmarkMalloc.h"
#include "BenchmarkAHM.h"

int main()
{
    std::cout << "========================================\n";
    std::cout << "TestApp - Memory Allocation Benchmark\n";
    std::cout << "========================================\n\n";

    std::vector<int> threadCounts = { 1, 2, 5, 10, 50 };
    std::vector<BenchmarkResult> mallocResults;
    std::vector<BenchmarkResult> ahmResults;

    // Pokretanje benchmarka
    run_malloc_benchmark(threadCounts, mallocResults);
    run_ahm_benchmark(threadCounts, ahmResults);

    // Prikaz rezultata poreðeno po broju niti
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Results Comparison\n";
    std::cout << "========================================\n";

    std::cout << std::left << std::setw(10) << "Threads"
        << std::setw(12) << "malloc(s)"
        << std::setw(12) << "AHM(s)"
        << std::setw(12) << "Speedup"
        << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (size_t i = 0; i < threadCounts.size(); i++)
    {
        double speedup = mallocResults[i].timeSeconds / ahmResults[i].timeSeconds;

        std::cout << std::left << std::setw(10) << threadCounts[i]
            << std::fixed << std::setprecision(3)
            << std::setw(12) << mallocResults[i].timeSeconds
            << std::setw(12) << ahmResults[i].timeSeconds
            << std::setprecision(2)
            << std::setw(12) << speedup
            << "\n";
    }

    std::cout << "\nBenchmark completed!\n";
    return 0;
}
