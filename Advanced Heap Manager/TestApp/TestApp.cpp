#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <string>
#include "BenchmarkMalloc.h"
#include "BenchmarkAHM.h"

// Number of times each configuration is repeated (the median is used in the table)
static const int REPETITIONS = 3;

using Times = std::vector<std::vector<double>>;   // Times[threadIndex] = all recorded measurements

static double median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

static void collect(const std::vector<BenchmarkResult>& r, Times& times)
{
    for (size_t i = 0; i < r.size(); ++i)
        times[i].push_back(r[i].timeSeconds);
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "TestApp - Memory Allocation Benchmark\n";
    std::cout << "========================================\n\n";

    std::vector<int> threadCounts = { 1, 2, 5, 10, 50 };

    Times tMalloc(threadCounts.size());
    Times tMap(threadCounts.size());
    Times tHeader(threadCounts.size());

    for (int rep = 1; rep <= REPETITIONS; ++rep)
    {
        std::cout << "\n##### Repetition " << rep << "/" << REPETITIONS << " #####\n";

        std::vector<BenchmarkResult> r;

        r.clear();
        run_malloc_benchmark(threadCounts, r);
        collect(r, tMalloc);

        r.clear();
        run_ahm_benchmark(threadCounts, r, AHM::Strategy::Map);
        collect(r, tMap);

        r.clear();
        run_ahm_benchmark(threadCounts, r, AHM::Strategy::Header);
        collect(r, tHeader);
    }

    // ---------- Results table (medians) ----------
    std::cout << "\n========================================\n";
    std::cout << "Results (median of " << REPETITIONS << " repetitions)\n";
    std::cout << "========================================\n";

    std::cout << std::left << std::setw(9) << "Threads"
        << std::setw(12) << "malloc(s)"
        << std::setw(12) << "Map(s)"
        << std::setw(12) << "Header(s)"
        << std::setw(14) << "Map/malloc"
        << std::setw(16) << "Header/malloc"
        << std::setw(12) << "Header/Map"
        << "\n";
    std::cout << std::string(87, '-') << "\n";

    for (size_t i = 0; i < threadCounts.size(); ++i)
    {
        const double m = median(tMalloc[i]);
        const double mp = median(tMap[i]);
        const double h = median(tHeader[i]);

        std::cout << std::left << std::setw(9) << threadCounts[i]
            << std::fixed << std::setprecision(3)
            << std::setw(12) << m
            << std::setw(12) << mp
            << std::setw(12) << h
            << std::setprecision(2)
            << std::setw(14) << (m / mp)
            << std::setw(16) << (m / h)
            << std::setw(12) << (mp / h)
            << "\n";
    }
    std::cout << "\n(Values > 1.00 in the last three columns indicate that the strategy is faster than the one in the column name)\n";

    // ---------- CSV export (one row = strategy + thread count) ----------
    std::ofstream csv("results.csv");
    csv << "strategy,threads,median_s,min_s,max_s\n";
    csv << std::fixed << std::setprecision(4);

    auto dump = [&](const char* name, const Times& t)
        {
            for (size_t i = 0; i < threadCounts.size(); ++i)
            {
                csv << name << "," << threadCounts[i] << ","
                    << median(t[i]) << ","
                    << *std::min_element(t[i].begin(), t[i].end()) << ","
                    << *std::max_element(t[i].begin(), t[i].end()) << "\n";
            }
        };
    dump("malloc", tMalloc);
    dump("ahm_map", tMap);
    dump("ahm_header", tHeader);
    csv.close();

    std::cout << "\nResults saved to results.csv\n";
    std::cout << "Benchmark completed!\n";
    return 0;
}