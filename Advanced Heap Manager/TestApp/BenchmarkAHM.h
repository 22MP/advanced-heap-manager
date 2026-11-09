#pragma once
#include <vector>
#include "BenchmarkTypes.h"
#include "AdvancedHeapManager.h"

// Runs the benchmark using the Advanced Heap Manager with the specified strategy
void run_ahm_benchmark(const std::vector<int>& threadCounts,
    std::vector<BenchmarkResult>& results,
    AHM::Strategy strategy = AHM::Strategy::Map);