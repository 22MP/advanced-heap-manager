#pragma once
#include <vector>
#include "BenchmarkTypes.h"

// Pokrece benchmark koristeci Advanced Heap Manager
void run_ahm_benchmark(const std::vector<int>& threadCounts, std::vector<BenchmarkResult>& results);
