#pragma once

// Benchmark funkcije za testiranje performansi
// malloc/free vs AHM (Advanced Heap Manager)

// Pokreæe benchmark test koristeæi standardni malloc/free
void run_malloc_benchmark();

// Pokreæe benchmark test koristeæi AHM
void run_ahm_benchmark();

// Pokreæe uporedni test (oba istovremeno) i ispisuje razlike
void run_comparison_benchmark();