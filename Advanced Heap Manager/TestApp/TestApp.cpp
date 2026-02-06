#include "Benchmark.h"
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "========================================\n";
    std::cout << "TestApp - Memory Allocation Benchmark\n";
    std::cout << "========================================\n\n";

    if (argc > 1)
    {
        std::string mode = argv[1];

        if (mode == "malloc")
        {
            std::cout << "Mode: malloc/free only\n";
            run_malloc_benchmark();
        }
        else if (mode == "ahm")
        {
            std::cout << "Mode: AHM only\n";
            run_ahm_benchmark();
        }
        else if (mode == "compare")
        {
            std::cout << "Mode: Comparison (malloc vs AHM)\n";
            run_comparison_benchmark();
        }
        else
        {
            std::cout << "Unknown mode: " << mode << "\n";
            std::cout << "Usage: TestApp.exe [malloc|ahm|compare]\n";
            return 1;
        }
    }
    else
    {
        // Default: pokreni sve testove
        std::cout << "Mode: All tests (default)\n\n";

        //run_malloc_benchmark();
        //run_ahm_benchmark();
        run_comparison_benchmark();
    }

    std::cout << "========================================\n";
    std::cout << "Benchmark completed!\n";
    std::cout << "========================================\n";

    return 0;
}