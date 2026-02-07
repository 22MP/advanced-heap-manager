#include "Benchmark.h"
#include <iostream>
#include <iomanip>
#include <ctime>

void PrintResults(const BenchmarkResult& r)
{
    std::cout << "\n========================================\n";
    std::cout << "Mode: " << (r.useAHM ? "AHM" : "malloc/free") << "\n";
    std::cout << "Time: " << std::fixed << std::setprecision(2)
        << r.elapsedSeconds << " s\n";
    std::cout << "Messages: " << r.totalMessages << "\n";
    std::cout << "Throughput: "
        << (r.totalMessages / r.elapsedSeconds) << " msg/sec\n";
}

int main()
{
    srand((unsigned)time(nullptr));

    int clients, messages;
    std::cout << "Clients: "; std::cin >> clients;
    std::cout << "Messages per client: "; std::cin >> messages;

    auto ahm = RunBenchmark(true, clients, messages);
    auto mal = RunBenchmark(false, clients, messages);

    PrintResults(ahm);
    PrintResults(mal);

    std::cout << "\nAHM faster by: " << ((mal.elapsedSeconds - ahm.elapsedSeconds) / mal.elapsedSeconds * 100.0) << " %\n";

    std::cin.ignore(); std::cin.get();
    return 0;
}
