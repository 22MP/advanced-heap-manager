#pragma once
#include <vector>
#include "BenchmarkTypes.h"


// Pokrece benchmark koristeci standardni malloc/free
void run_malloc_benchmark(const std::vector<int>& threadCounts, std::vector<BenchmarkResult>& results);
