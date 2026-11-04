#pragma once
#include <cstddef>

namespace AHM
{
    enum class Strategy
    {
        Map,     // ptr -> heapId preko AllocationMap (originalno rješenje)
        Header   // heapId u 16B zaglavlju ispred bloka
    };

    void Initialize(size_t heapCount, size_t heapSizeBytes,
        Strategy strategy = Strategy::Map);
    void Shutdown();

    void* Malloc(size_t sizeBytes);
    void  Free(void* ptr);
}