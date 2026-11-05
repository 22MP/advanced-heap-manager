#pragma once
#include <vector>
#include "BenchmarkTypes.h"
#include "AdvancedHeapManager.h"

// Pokrece benchmark koristeci Advanced Heap Manager sa zadatom strategijom
void run_ahm_benchmark(const std::vector<int>& threadCounts,
    std::vector<BenchmarkResult>& results,
    AHM::Strategy strategy = AHM::Strategy::Map);