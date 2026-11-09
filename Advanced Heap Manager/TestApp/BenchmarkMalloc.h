#pragma once
#include <vector>
#include "BenchmarkTypes.h"


// Runs the benchmark using standard malloc/free
void run_malloc_benchmark(const std::vector<int>& threadCounts, std::vector<BenchmarkResult>& results);